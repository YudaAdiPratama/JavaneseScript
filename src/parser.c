/* Parser: nyusun token dadi wit sintaksis (AST). */
#include <stdlib.h>
#include <string.h>

#include "jws.h"

typedef struct {
    TokenList tl;
    int pos;
} Parser;

static Node *statement(Parser *p);
static Node *expression(Parser *p);

static Token *peek_at(Parser *p, int offset) {
    int i = p->pos + offset;
    if (i >= p->tl.len) i = p->tl.len - 1;
    return &p->tl.items[i];
}
static Token *peek(Parser *p) { return peek_at(p, 0); }
static Token *next(Parser *p) {
    Token *t = peek(p);
    if (p->pos < p->tl.len - 1) p->pos++;
    return t;
}
static Token *prev(Parser *p) { return &p->tl.items[p->pos - 1]; }

/* Teks lan op wis di-intern, mula cukup mbandhingake isine sepisan. */
static bool tok_is(Token *t, TokType type, const char *value) {
    if (t->type != type) return false;
    return value == NULL || strcmp(t->text, value) == 0;
}
static bool check(Parser *p, TokType type, const char *value) { return tok_is(peek(p), type, value); }
static bool match(Parser *p, TokType type, const char *value) {
    if (!check(p, type, value)) return false;
    next(p);
    return true;
}

/* Teks token kanggo pesen kesalahan, padha karo versi asline. */
static void describe(Token *t, Buf *b) {
    if (t->type == T_EOF) {
        buf_puts(b, "pungkasan berkas");
        return;
    }
    buf_push(b, '"');
    if (t->type == T_NUM) number_text(b, t->num);
    else buf_append(b, t->text, t->len);
    buf_push(b, '"');
}

static void fail_found(Token *t, const char *prefix) {
    Buf b = {0};
    buf_puts(&b, prefix);
    describe(t, &b);
    char *msg = buf_take(&b);
    jws_error(t->line, "%s", msg);
}

static Token *expect(Parser *p, TokType type, const char *value, const char *what) {
    if (check(p, type, value)) return next(p);
    Buf b = {0};
    buf_printf(&b, "dikarepake %s, nanging ketemu ", what);
    char *prefix = buf_take(&b);
    fail_found(peek(p), prefix);
    return NULL;
}

static Node *node_new(NodeKind kind, int line) {
    Node *n = calloc(1, sizeof *n);
    n->kind = kind;
    n->line = line;
    return n;
}

static void list_add(NodeList *l, Node *n) {
    if (l->len == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 4;
        l->items = realloc(l->items, sizeof(Node *) * l->cap);
    }
    l->items[l->len++] = n;
}

static void name_add(NameList *l, const char *s) {
    if (l->len == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 4;
        l->items = realloc(l->items, sizeof(char *) * l->cap);
    }
    l->items[l->len++] = s;
}

static void skip_ends(Parser *p) {
    while (match(p, T_END, NULL));
}

static NodeList block(Parser *p) {
    expect(p, T_OP, "{", "\"{\"");
    NodeList body = {0};
    skip_ends(p);
    while (!check(p, T_OP, "}")) {
        if (check(p, T_EOF, NULL)) jws_error(peek(p)->line, "blok ora ditutup nganggo \"}\"");
        list_add(&body, statement(p));
        skip_ends(p);
    }
    next(p);
    return body;
}

static Node *function_rest(Parser *p, const char *name, int line) {
    Node *n = node_new(N_FUNCTION, line);
    n->name = name;
    expect(p, T_OP, "(", "\"(\"");
    if (!check(p, T_OP, ")")) {
        do {
            name_add(&n->params, expect(p, T_IDENT, NULL, "jeneng parameter")->text);
        } while (match(p, T_OP, ","));
    }
    expect(p, T_OP, ")", "\")\"");
    n->body = block(p);
    return n;
}

static Node *if_rest(Parser *p, int line) {
    Node *n = node_new(N_IF, line);
    n->a = expression(p);
    n->body = block(p);
    if (match(p, T_KW, "liyane")) {
        n->has_alt = true;
        if (match(p, T_KW, "yen")) list_add(&n->alt, if_rest(p, peek(p)->line));
        else n->alt = block(p);
    }
    return n;
}

static Node *statement(Parser *p) {
    Token *t = peek(p);
    if (t->type == T_KW) {
        const char *kw = t->text;
        if (strcmp(kw, "simpen") == 0) {
            next(p);
            Node *n = node_new(N_LET, t->line);
            n->name = expect(p, T_IDENT, NULL, "jeneng variabel")->text;
            expect(p, T_OP, "=", "\"=\"");
            n->a = expression(p);
            return n;
        }
        if (strcmp(kw, "tulis") == 0) {
            next(p);
            Node *n = node_new(N_PRINT, t->line);
            list_add(&n->body, expression(p));
            while (match(p, T_OP, ",")) list_add(&n->body, expression(p));
            return n;
        }
        if (strcmp(kw, "yen") == 0) {
            next(p);
            return if_rest(p, t->line);
        }
        if (strcmp(kw, "salawase") == 0) {
            next(p);
            Node *n = node_new(N_WHILE, t->line);
            n->a = expression(p);
            n->body = block(p);
            return n;
        }
        if (strcmp(kw, "saben") == 0) {
            next(p);
            Node *n = node_new(N_FOREACH, t->line);
            n->name = expect(p, T_IDENT, NULL, "jeneng variabel")->text;
            expect(p, T_KW, "ing", "\"ing\"");
            n->a = expression(p);
            n->body = block(p);
            return n;
        }
        if (strcmp(kw, "gawe") == 0 && peek_at(p, 1)->type == T_IDENT) {
            next(p);
            const char *name = next(p)->text;
            Node *n = node_new(N_LET, t->line);
            n->name = name;
            n->a = function_rest(p, name, t->line);
            return n;
        }
        if (strcmp(kw, "balekna") == 0) {
            next(p);
            Node *n = node_new(N_RETURN, t->line);
            Token *nt = peek(p);
            bool bare = nt->type == T_END || nt->type == T_EOF || tok_is(nt, T_OP, "}") || nt->line != t->line;
            n->a = bare ? NULL : expression(p);
            return n;
        }
        if (strcmp(kw, "mandheg") == 0) {
            next(p);
            return node_new(N_BREAK, t->line);
        }
        if (strcmp(kw, "terus") == 0) {
            next(p);
            return node_new(N_CONTINUE, t->line);
        }
    }

    Node *expr = expression(p);
    if (check(p, T_OP, "=")) {
        Token *eq = next(p);
        if (expr->kind != N_IDENT && expr->kind != N_INDEX) {
            jws_error(eq->line, "sisih kiwa \"=\" kudu variabel utawa indeks");
        }
        Node *n = node_new(N_ASSIGN, eq->line);
        n->a = expr;
        n->b = expression(p);
        return n;
    }
    Node *n = node_new(N_EXPR, t->line);
    n->a = expr;
    return n;
}

/* ---------- ekspresi ---------- */

static Node *binary_node(NodeKind kind, OpKind op, Node *left, Node *right, int line) {
    Node *n = node_new(kind, line);
    n->op = op;
    n->a = left;
    n->b = right;
    return n;
}

static Node *unary(Parser *p);
static Node *not_expr(Parser *p);

typedef struct {
    const char *text;
    OpKind op;
} OpEntry;

static Node *binary_level(Parser *p, const OpEntry *ops, Node *(*next_level)(Parser *)) {
    Node *left = next_level(p);
    for (;;) {
        Token *t = peek(p);
        if (t->type != T_OP) return left;
        const OpEntry *found = NULL;
        for (const OpEntry *e = ops; e->text; e++) {
            if (strcmp(e->text, t->text) == 0) found = e;
        }
        if (!found) return left;
        next(p);
        Node *right = next_level(p);
        left = binary_node(N_BINARY, found->op, left, right, t->line);
    }
}

static const OpEntry MUL_OPS[] = {{"*", OP_MUL}, {"/", OP_DIV}, {"%", OP_MOD}, {NULL, 0}};
static const OpEntry ADD_OPS[] = {{"+", OP_ADD}, {"-", OP_SUB}, {NULL, 0}};
static const OpEntry CMP_OPS[] = {{"<", OP_LT}, {"<=", OP_LE}, {">", OP_GT}, {">=", OP_GE}, {NULL, 0}};
static const OpEntry EQ_OPS[] = {{"==", OP_EQ}, {"!=", OP_NE}, {NULL, 0}};

static Node *multiplicative(Parser *p) { return binary_level(p, MUL_OPS, unary); }
static Node *additive(Parser *p) { return binary_level(p, ADD_OPS, multiplicative); }
static Node *comparison(Parser *p) { return binary_level(p, CMP_OPS, additive); }
static Node *equality(Parser *p) { return binary_level(p, EQ_OPS, comparison); }

static Node *not_expr(Parser *p) {
    if (check(p, T_KW, "ora")) {
        Token *t = next(p);
        Node *n = node_new(N_UNARY, t->line);
        n->op = OP_NOT;
        n->a = not_expr(p);
        return n;
    }
    return equality(p);
}

static Node *and_expr(Parser *p) {
    Node *left = not_expr(p);
    while (check(p, T_KW, "lan")) {
        Token *t = next(p);
        Node *right = not_expr(p);
        left = binary_node(N_LOGICAL, OP_AND, left, right, t->line);
    }
    return left;
}

static Node *or_expr(Parser *p) {
    Node *left = and_expr(p);
    while (check(p, T_KW, "utawa")) {
        Token *t = next(p);
        Node *right = and_expr(p);
        left = binary_node(N_LOGICAL, OP_OR, left, right, t->line);
    }
    return left;
}

static Node *expression(Parser *p) { return or_expr(p); }

static Node *primary(Parser *p) {
    Token *t = next(p);
    Node *n;
    switch (t->type) {
    case T_NUM:
        n = node_new(N_LITERAL, t->line);
        n->lit = num_val(t->num);
        return n;
    case T_STR:
        n = node_new(N_LITERAL, t->line);
        n->lit = str_val(str_new_permanent(t->text, t->len));
        return n;
    case T_IDENT:
        n = node_new(N_IDENT, t->line);
        n->name = t->text;
        return n;
    case T_KW:
        if (strcmp(t->text, "bener") == 0 || strcmp(t->text, "luput") == 0) {
            n = node_new(N_LITERAL, t->line);
            n->lit = bool_val(t->text[0] == 'b');
            return n;
        }
        if (strcmp(t->text, "kosong") == 0) {
            n = node_new(N_LITERAL, t->line);
            n->lit = NULL_VAL;
            return n;
        }
        if (strcmp(t->text, "gawe") == 0) return function_rest(p, NULL, t->line);
        break;
    case T_OP:
        if (strcmp(t->text, "(") == 0) {
            Node *e = expression(p);
            expect(p, T_OP, ")", "\")\"");
            return e;
        }
        if (strcmp(t->text, "[") == 0) {
            n = node_new(N_LIST, t->line);
            if (!check(p, T_OP, "]")) {
                do {
                    list_add(&n->body, expression(p));
                } while (match(p, T_OP, ","));
            }
            expect(p, T_OP, "]", "\"]\"");
            return n;
        }
        break;
    default:
        break;
    }
    fail_found(t, "ekspresi ora sah, ketemu ");
    return NULL;
}

/* Panggilan lan indeks kudu ing baris sing padha, supaya ukara anyar
 * sing diwiwiti "(" utawa "[" ora kacampur karo ukara sadurunge. */
static Node *postfix(Parser *p) {
    Node *expr = primary(p);
    for (;;) {
        Token *t = peek(p);
        if (tok_is(t, T_OP, "(") && t->line == prev(p)->line) {
            next(p);
            Node *n = node_new(N_CALL, t->line);
            n->a = expr;
            if (!check(p, T_OP, ")")) {
                do {
                    list_add(&n->body, expression(p));
                } while (match(p, T_OP, ","));
            }
            expect(p, T_OP, ")", "\")\"");
            expr = n;
        } else if (tok_is(t, T_OP, "[") && t->line == prev(p)->line) {
            next(p);
            Node *n = node_new(N_INDEX, t->line);
            n->a = expr;
            n->b = expression(p);
            expect(p, T_OP, "]", "\"]\"");
            expr = n;
        } else {
            return expr;
        }
    }
}

static Node *unary(Parser *p) {
    if (check(p, T_OP, "-")) {
        Token *t = next(p);
        Node *n = node_new(N_UNARY, t->line);
        n->op = OP_NEG;
        n->a = unary(p);
        return n;
    }
    return postfix(p);
}

Program parse(const char *src) {
    Parser p = {tokenize(src), 0};
    Program prog = {0};
    skip_ends(&p);
    while (!check(&p, T_EOF, NULL)) {
        list_add(&prog.body, statement(&p));
        skip_ends(&p);
    }
    for (int i = 0; i < p.tl.len; i++) {
        if (p.tl.items[i].type == T_STR) free((char *)p.tl.items[i].text);
    }
    free(p.tl.items);
    return prog;
}
