/* Interpreter: nglakokake wit sintaksis (AST). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jws.h"

typedef enum { ST_OK, ST_BREAK, ST_CONTINUE, ST_RETURN } Status;

struct Interp {
    Env *globals;
    Value ret;       /* nilai saka balekna */
    int signal_line; /* baris mandheg/terus */
    const char *stack_base;
    size_t stack_budget;
    int root_base;
};

static Status exec(Interp *in, Node *n, Env *env);
static Value eval(Interp *in, Node *n, Env *env);

static bool truthy(Value v) {
    switch (v.type) {
    case V_NULL: return false;
    case V_BOOL: return v.as.b;
    case V_NUM: return v.as.n != 0;
    case V_STR: return v.as.s->len != 0;
    default: return true;
    }
}

/* Format nilai menyang tembung anyar (malloc). */
static char *fmt(Value v, bool nested) {
    Buf b = {0};
    format_value(&b, v, nested);
    return buf_take(&b);
}

static Value env_value(Env *e) {
    Value v = {V_ENV, {.e = e}};
    return v;
}

/* ---------- fungsi bawaan ---------- */

typedef Value (*BuiltinFn)(Interp *in, Value *args, int line);

typedef struct {
    const char *name;
    int arity;
    BuiltinFn fn;
} Builtin;

static const Builtin BUILTINS[];

const char *builtin_name(int index) { return BUILTINS[index].name; }

/* Teks polos saka nilai (daftar dipisah koma, kosong dadi teks kosong), dienggo angka(). */
static void plain_text(Buf *b, Value v) {
    switch (v.type) {
    case V_NULL: buf_puts(b, "null"); break;
    case V_BOOL: buf_puts(b, v.as.b ? "true" : "false"); break;
    case V_NUM: number_text(b, v.as.n); break;
    case V_STR: buf_append(b, v.as.s->data, v.as.s->len); break;
    case V_LIST:
        for (size_t i = 0; i < v.as.l->len; i++) {
            if (i) buf_push(b, ',');
            if (v.as.l->items[i].type != V_NULL) plain_text(b, v.as.l->items[i]);
        }
        break;
    default: buf_puts(b, "function"); break;
    }
}

static bool is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
static bool is_dig(char c) { return c >= '0' && c <= '9'; }

/* Ngurai teks dadi angka (desimal, eksponen, 0x/0o/0b, Infinity).
 * Ngasilake false yen dudu angka. */
static bool parse_number(const char *s, size_t n, double *out) {
    if (n == 3 + 5 && memcmp(s, "Infinity", 8) == 0) return *out = INFINITY, true;
    if (n == 9 && memcmp(s, "+Infinity", 9) == 0) return *out = INFINITY, true;
    if (n == 9 && memcmp(s, "-Infinity", 9) == 0) return *out = -INFINITY, true;

    if (n > 2 && s[0] == '0' && strchr("xXoObB", s[1])) {
        int base = (s[1] == 'x' || s[1] == 'X') ? 16 : (s[1] == 'o' || s[1] == 'O') ? 8 : 2;
        double v = 0;
        for (size_t i = 2; i < n; i++) {
            int d;
            char c = s[i];
            if (is_dig(c)) d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            if (d >= base) return false;
            v = v * base + d;
        }
        *out = v;
        return true;
    }

    size_t i = 0;
    if (i < n && (s[i] == '+' || s[i] == '-')) i++;
    size_t int_digits = 0, frac_digits = 0;
    while (i < n && is_dig(s[i])) i++, int_digits++;
    if (i < n && s[i] == '.') {
        i++;
        while (i < n && is_dig(s[i])) i++, frac_digits++;
    }
    if (int_digits + frac_digits == 0) return false;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < n && (s[i] == '+' || s[i] == '-')) i++;
        size_t exp_digits = 0;
        while (i < n && is_dig(s[i])) i++, exp_digits++;
        if (exp_digits == 0) return false;
    }
    if (i != n) return false;
    char *tmp = malloc(n + 1);
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    *out = strtod(tmp, NULL);
    free(tmp);
    return true;
}

static Value bi_dawa(Interp *in, Value *a, int line) {
    (void)in;
    if (a[0].type == V_STR) return num_val((double)utf8_count(a[0].as.s->data, a[0].as.s->len));
    if (a[0].type == V_LIST) return num_val((double)a[0].as.l->len);
    jws_error(line, "dawa butuh tembung utawa daftar, ora %s", type_name(a[0]));
    return NULL_VAL;
}

static Value bi_lebokna(Interp *in, Value *a, int line) {
    (void)in;
    if (a[0].type != V_LIST) jws_error(line, "lebokna butuh daftar");
    list_push(a[0].as.l, a[1]);
    return a[0];
}

static Value bi_angka(Interp *in, Value *a, int line) {
    (void)in;
    if (a[0].type == V_NUM) return a[0];
    Buf b = {0};
    plain_text(&b, a[0]);
    size_t start = 0, end = b.len;
    while (start < end && is_ws(b.data[start])) start++;
    while (end > start && is_ws(b.data[end - 1])) end--;
    double v = 0;
    if (start == end || !parse_number(b.data + start, end - start, &v) || isnan(v)) {
        char *s = buf_take(&b);
        jws_error(line, "\"%s\" dudu angka", s);
    }
    free(b.data);
    return num_val(v);
}

static Value bi_tembung(Interp *in, Value *a, int line) {
    (void)in;
    (void)line;
    Buf b = {0};
    format_value(&b, a[0], false);
    Value v = str_val(str_new(b.data ? b.data : "", b.len));
    free(b.data);
    return v;
}

static Value bi_urutan(Interp *in, Value *a, int line) {
    (void)in;
    if (a[0].type != V_NUM || a[1].type != V_NUM) jws_error(line, "urutan butuh angka");
    List *l = list_new(0);
    for (double i = a[0].as.n; i < a[1].as.n; i++) list_push(l, num_val(i));
    return list_val(l);
}

static Value bi_waca(Interp *in, Value *a, int line) {
    (void)in;
    (void)line;
    if (a[0].type != V_NULL) {
        char *s = fmt(a[0], false);
        fputs(s, stdout);
        free(s);
    }
    fflush(stdout);
    Buf b = {0};
    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') buf_push(&b, (char)c);
    if (b.len && b.data[b.len - 1] == '\r') b.len--;
    Value v = str_val(str_new(b.data ? b.data : "", b.len));
    free(b.data);
    return v;
}

static Value bi_jinis(Interp *in, Value *a, int line) {
    (void)in;
    (void)line;
    const char *t = type_name(a[0]);
    return str_val(str_new(t, strlen(t)));
}

static const Builtin BUILTINS[] = {
    {"dawa", 1, bi_dawa},       /* dawane daftar utawa tembung */
    {"lebokna", 2, bi_lebokna}, /* nambah isi ing pungkasan daftar */
    {"angka", 1, bi_angka},     /* ngowahi tembung dadi angka */
    {"tembung", 1, bi_tembung}, /* ngowahi nilai dadi tembung */
    {"urutan", 2, bi_urutan},   /* daftar angka saka a nganti sadurunge b */
    {"waca", 1, bi_waca},       /* maca siji baris saka panganggo */
    {"jinis", 1, bi_jinis},     /* jinise nilai */
    {NULL, 0, NULL},
};

/* ---------- operasi ---------- */

static const char *OP_TEXT[] = {"+", "-", "*", "/", "%", "==", "!=", "<", "<=", ">", ">="};

static int str_compare(Str *a, Str *b) {
    size_t n = a->len < b->len ? a->len : b->len;
    int c = memcmp(a->data, b->data, n);
    if (c) return c;
    return a->len < b->len ? -1 : a->len > b->len ? 1 : 0;
}

static Value binary(OpKind op, Value a, Value b, int line) {
    bool nums = a.type == V_NUM && b.type == V_NUM;
    switch (op) {
    case OP_EQ: return bool_val(values_equal(a, b));
    case OP_NE: return bool_val(!values_equal(a, b));
    case OP_ADD:
        if (nums) return num_val(a.as.n + b.as.n);
        if (a.type == V_STR || b.type == V_STR) {
            Buf buf = {0};
            format_value(&buf, a, false);
            format_value(&buf, b, false);
            Value v = str_val(str_new(buf.data ? buf.data : "", buf.len));
            free(buf.data);
            return v;
        }
        if (a.type == V_LIST && b.type == V_LIST) {
            List *l = list_new(a.as.l->len + b.as.l->len);
            for (size_t i = 0; i < a.as.l->len; i++) list_push(l, a.as.l->items[i]);
            for (size_t i = 0; i < b.as.l->len; i++) list_push(l, b.as.l->items[i]);
            return list_val(l);
        }
        break;
    default: break;
    }

    if (op == OP_LT || op == OP_LE || op == OP_GT || op == OP_GE) {
        int c;
        if (nums) {
            double x = a.as.n, y = b.as.n;
            if (op == OP_LT) return bool_val(x < y);
            if (op == OP_LE) return bool_val(x <= y);
            if (op == OP_GT) return bool_val(x > y);
            return bool_val(x >= y);
        }
        if (a.type == V_STR && b.type == V_STR) {
            c = str_compare(a.as.s, b.as.s);
            if (op == OP_LT) return bool_val(c < 0);
            if (op == OP_LE) return bool_val(c <= 0);
            if (op == OP_GT) return bool_val(c > 0);
            return bool_val(c >= 0);
        }
    }

    if (nums) {
        double x = a.as.n, y = b.as.n;
        if (op == OP_SUB) return num_val(x - y);
        if (op == OP_MUL) return num_val(x * y);
        if ((op == OP_DIV || op == OP_MOD) && y == 0) jws_error(line, "ora bisa dibagi nol");
        if (op == OP_DIV) return num_val(x / y);
        if (op == OP_MOD) return num_val(fmod(x, y));
    }

    if (op == OP_MUL && a.type == V_STR && b.type == V_NUM && b.as.n >= 0 && b.as.n == floor(b.as.n) &&
        isfinite(b.as.n)) {
        double total = (double)a.as.s->len * b.as.n;
        if (total > (double)(1u << 30)) jws_error(line, "tembung kedawan");
        size_t times = (size_t)b.as.n, len = a.as.s->len;
        Buf buf = {0};
        for (size_t i = 0; i < times; i++) buf_append(&buf, a.as.s->data, len);
        Value v = str_val(str_new(buf.data ? buf.data : "", buf.len));
        free(buf.data);
        return v;
    }

    jws_error(line, "operasi \"%s\" ora bisa kanggo %s lan %s", OP_TEXT[op], type_name(a), type_name(b));
    return NULL_VAL;
}

/* Mriksa indeks: kudu wilangan bunder ing jero wates. */
static size_t check_index(Value idx, size_t len, int line) {
    if (idx.type != V_NUM || idx.as.n != floor(idx.as.n) || !isfinite(idx.as.n) || idx.as.n < 0 ||
        idx.as.n >= (double)len) {
        char *s = fmt(idx, false);
        Buf b = {0};
        number_text(&b, (double)len);
        jws_error(line, "indeks %s ing njaba wates (dawa %s)", s, b.data);
    }
    return (size_t)idx.as.n;
}

/* Golek posisi bait karakter kaping-i ing tembung UTF-8. */
static size_t utf8_offset(Str *s, size_t index) {
    size_t i = 0;
    while (index-- > 0) i += utf8_seq_len((unsigned char)s->data[i]);
    return i;
}

static Value char_at(Str *s, size_t offset) {
    size_t len = utf8_seq_len((unsigned char)s->data[offset]);
    if (offset + len > s->len) len = s->len - offset;
    return str_val(str_new(s->data + offset, len));
}

/* ---------- panggilan ---------- */

static Status exec_block(Interp *in, NodeList *body, Env *env) {
    root_push(env_value(env));
    Status st = ST_OK;
    for (int i = 0; i < body->len; i++) {
        st = exec(in, body->items[i], env);
        if (st != ST_OK) break;
    }
    root_pop(1);
    return st;
}

static Value call(Interp *in, Node *n, Env *env) {
    Value callee = eval(in, n->a, env);
    root_push(callee);
    int argc = n->body.len;
    Value *args = malloc(sizeof(Value) * (argc ? argc : 1));
    for (int i = 0; i < argc; i++) {
        args[i] = eval(in, n->body.items[i], env);
        root_push(args[i]);
    }

    Value result = NULL_VAL;
    if (callee.type == V_BUILTIN) {
        const Builtin *b = &BUILTINS[callee.as.builtin];
        if (argc != b->arity) {
            free(args);
            jws_error(n->line, "%s butuh %d argumen, nanging oleh %d", b->name, b->arity, argc);
        }
        result = b->fn(in, args, n->line);
    } else if (callee.type == V_FUNC) {
        Node *fn = callee.as.f->node;
        if (argc != fn->params.len) {
            free(args);
            jws_error(n->line, "%s butuh %d argumen, nanging oleh %d", fn->name ? fn->name : "gawe", fn->params.len,
                      argc);
        }
        char here;
        if (in->stack_base && (size_t)(in->stack_base - &here) > in->stack_budget) {
            free(args);
            jws_error(n->line, "rekursi kakehan (stack overflow)");
        }
        Env *local = env_new(callee.as.f->closure);
        for (int i = 0; i < argc; i++) env_define(local, fn->params.items[i], args[i]);
        Status st = exec_block(in, &fn->body, local);
        if (st == ST_RETURN) {
            result = in->ret;
            in->ret = NULL_VAL;
        } else if (st == ST_BREAK || st == ST_CONTINUE) {
            free(args);
            jws_error(n->line, "mandheg/terus ing njaba puteran");
        }
    } else {
        free(args);
        jws_error(n->line, "%s dudu gawe, ora bisa diundang", type_name(callee));
    }
    free(args);
    root_pop(argc + 1);
    return result;
}

/* ---------- ekspresi ---------- */

static Value eval(Interp *in, Node *n, Env *env) {
    switch (n->kind) {
    case N_LITERAL: return n->lit;
    case N_IDENT: {
        int idx;
        Env *e = env_find(env, n->name, &idx);
        if (!e) jws_error(n->line, "variabel \"%s\" durung ditetepake", n->name);
        return e->vals[idx];
    }
    case N_LIST: {
        List *l = list_new((size_t)n->body.len);
        root_push(list_val(l));
        for (int i = 0; i < n->body.len; i++) list_push(l, eval(in, n->body.items[i], env));
        root_pop(1);
        return list_val(l);
    }
    case N_FUNCTION: {
        Value v = {V_FUNC, {.f = func_new(n, env)}};
        return v;
    }
    case N_INDEX: {
        Value obj = eval(in, n->a, env);
        root_push(obj);
        Value idx = eval(in, n->b, env);
        root_pop(1);
        if (obj.type == V_STR) {
            size_t i = check_index(idx, utf8_count(obj.as.s->data, obj.as.s->len), n->line);
            return char_at(obj.as.s, utf8_offset(obj.as.s, i));
        }
        if (obj.type != V_LIST) jws_error(n->line, "ora bisa diindeks: %s", type_name(obj));
        return obj.as.l->items[check_index(idx, obj.as.l->len, n->line)];
    }
    case N_UNARY: {
        Value v = eval(in, n->a, env);
        if (n->op == OP_NOT) return bool_val(!truthy(v));
        if (v.type != V_NUM) jws_error(n->line, "\"-\" butuh angka, ora %s", type_name(v));
        return num_val(-v.as.n);
    }
    case N_LOGICAL: {
        Value left = eval(in, n->a, env);
        if (n->op == OP_AND) return truthy(left) ? eval(in, n->b, env) : left;
        return truthy(left) ? left : eval(in, n->b, env);
    }
    case N_BINARY: {
        Value left = eval(in, n->a, env);
        root_push(left);
        Value right = eval(in, n->b, env);
        root_pop(1);
        return binary(n->op, left, right, n->line);
    }
    case N_CALL: return call(in, n, env);
    default: jws_error(n->line, "ekspresi ora dikenal"); return NULL_VAL;
    }
}

/* ---------- ukara ---------- */

static Status loop_body(Interp *in, NodeList *body, Env *inner, bool *stop) {
    Status st = exec_block(in, body, inner);
    *stop = false;
    if (st == ST_BREAK) *stop = true;
    if (st == ST_RETURN) {
        *stop = true;
        return ST_RETURN;
    }
    return ST_OK;
}

static Status exec(Interp *in, Node *n, Env *env) {
    gc_maybe_collect();
    switch (n->kind) {
    case N_LET:
        env_define(env, n->name, eval(in, n->a, env));
        return ST_OK;
    case N_ASSIGN: {
        Value value = eval(in, n->b, env);
        if (n->a->kind == N_IDENT) {
            int idx;
            Env *e = env_find(env, n->a->name, &idx);
            if (!e) jws_error(n->line, "variabel \"%s\" durung ditetepake (gunakna simpen)", n->a->name);
            e->vals[idx] = value;
            return ST_OK;
        }
        root_push(value);
        Value obj = eval(in, n->a->a, env);
        root_push(obj);
        Value idx = eval(in, n->a->b, env);
        root_pop(2);
        if (obj.type != V_LIST) jws_error(n->line, "mung daftar sing bisa diowahi isine");
        obj.as.l->items[check_index(idx, obj.as.l->len, n->line)] = value;
        return ST_OK;
    }
    case N_PRINT: {
        int count = n->body.len;
        Value *vals = malloc(sizeof(Value) * count);
        for (int i = 0; i < count; i++) {
            vals[i] = eval(in, n->body.items[i], env);
            root_push(vals[i]);
        }
        Buf b = {0};
        for (int i = 0; i < count; i++) {
            if (i) buf_push(&b, ' ');
            format_value(&b, vals[i], false);
        }
        buf_push(&b, '\n');
        fwrite(b.data, 1, b.len, stdout);
        free(b.data);
        free(vals);
        root_pop(count);
        return ST_OK;
    }
    case N_IF:
        if (truthy(eval(in, n->a, env))) return exec_block(in, &n->body, env_new(env));
        if (n->has_alt) return exec_block(in, &n->alt, env_new(env));
        return ST_OK;
    case N_WHILE:
        while (truthy(eval(in, n->a, env))) {
            bool stop;
            Status st = loop_body(in, &n->body, env_new(env), &stop);
            if (st == ST_RETURN) return st;
            if (stop) break;
        }
        return ST_OK;
    case N_FOREACH: {
        Value it = eval(in, n->a, env);
        List *items;
        if (it.type == V_LIST) {
            items = list_new(it.as.l->len);
            for (size_t i = 0; i < it.as.l->len; i++) list_push(items, it.as.l->items[i]);
        } else if (it.type == V_STR) {
            root_push(it);
            items = list_new(0);
            root_push(list_val(items));
            for (size_t off = 0; off < it.as.s->len; off += utf8_seq_len((unsigned char)it.as.s->data[off])) {
                list_push(items, char_at(it.as.s, off));
            }
            root_pop(2);
        } else {
            jws_error(n->line, "ora bisa diulang: %s", type_name(it));
            return ST_OK;
        }
        root_push(list_val(items));
        Status result = ST_OK;
        for (size_t i = 0; i < items->len; i++) {
            Env *inner = env_new(env);
            env_define(inner, n->name, items->items[i]);
            bool stop;
            Status st = loop_body(in, &n->body, inner, &stop);
            if (st == ST_RETURN) {
                result = st;
                break;
            }
            if (stop) break;
        }
        root_pop(1);
        return result;
    }
    case N_RETURN:
        in->ret = n->a ? eval(in, n->a, env) : NULL_VAL;
        return ST_RETURN;
    case N_BREAK:
        in->signal_line = n->line;
        return ST_BREAK;
    case N_CONTINUE:
        in->signal_line = n->line;
        return ST_CONTINUE;
    case N_EXPR:
        eval(in, n->a, env);
        return ST_OK;
    default: jws_error(n->line, "ukara ora dikenal"); return ST_OK;
    }
}

/* ---------- API ---------- */

Interp *interp_new(void) {
    Interp *in = calloc(1, sizeof *in);
    in->globals = env_new(NULL);
    gc_set_globals(in->globals);
    for (int i = 0; BUILTINS[i].name; i++) {
        Value v = {V_BUILTIN, {.builtin = i}};
        env_define(in->globals, intern(BUILTINS[i].name, strlen(BUILTINS[i].name)), v);
    }
    in->root_base = root_mark();
    return in;
}

void interp_reset(Interp *in) {
    root_reset(in->root_base);
    in->ret = NULL_VAL;
}

void interp_set_stack(Interp *in, const char *base, size_t budget) {
    in->stack_base = base;
    in->stack_budget = budget;
}

Value interp_run(Interp *in, const char *src) {
    Program prog = parse(src);
    Value last = NULL_VAL;
    for (int i = 0; i < prog.body.len; i++) {
        Node *stmt = prog.body.items[i];
        last = NULL_VAL;
        if (stmt->kind == N_EXPR) {
            gc_maybe_collect();
            last = eval(in, stmt->a, in->globals);
            continue;
        }
        Status st = exec(in, stmt, in->globals);
        if (st == ST_RETURN) {
            in->ret = NULL_VAL;
            break;
        }
        if (st == ST_BREAK || st == ST_CONTINUE) jws_error(in->signal_line, "mandheg/terus ing njaba puteran");
    }
    free(prog.body.items);
    return last;
}
