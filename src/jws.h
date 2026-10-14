/* JavaneseScript - basa pemrograman nganggo basa Jawa. */
#ifndef JWS_H
#define JWS_H

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>

/* ---------- kesalahan ---------- */

/* Nyimpen "Kesalahan ing baris N: ..." ing jws_error_msg banjur longjmp
 * menyang jws_error_jmp. */
void jws_error(int line, const char *fmt, ...);
extern jmp_buf *jws_error_jmp;
extern char *jws_error_msg;

/* ---------- buffer tembung ---------- */

typedef struct {
    char *data;
    size_t len, cap;
} Buf;

void buf_push(Buf *b, char c);
void buf_append(Buf *b, const char *s, size_t n);
void buf_puts(Buf *b, const char *s);
void buf_printf(Buf *b, const char *fmt, ...);
char *buf_take(Buf *b); /* asile kudu di-free() */

/* ---------- token ---------- */

typedef enum { T_NUM, T_STR, T_IDENT, T_KW, T_OP, T_END, T_EOF } TokType;

typedef struct {
    TokType type;
    int line;
    const char *text; /* teks token (wis di-intern kanggo ident/kw/op) */
    size_t len;       /* dawane teks (kanggo tembung sing bisa ngemot \0) */
    double num;
} Token;

typedef struct {
    Token *items;
    int len, cap;
} TokenList;

const char *intern(const char *s, size_t n);
TokenList tokenize(const char *src);

/* ---------- AST ---------- */

typedef enum {
    N_LET, N_ASSIGN, N_PRINT, N_IF, N_WHILE, N_FOREACH, N_RETURN, N_BREAK, N_CONTINUE, N_EXPR,
    N_LITERAL, N_IDENT, N_LIST, N_FUNCTION, N_INDEX, N_UNARY, N_LOGICAL, N_BINARY, N_CALL
} NodeKind;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_AND, OP_OR, OP_NOT, OP_NEG
} OpKind;

typedef struct Node Node;

typedef struct {
    Node **items;
    int len, cap;
} NodeList;

typedef struct {
    const char **items;
    int len, cap;
} NameList;

/* ---------- nilai ---------- */

typedef enum { V_NULL, V_BOOL, V_NUM, V_STR, V_LIST, V_FUNC, V_BUILTIN, V_ENV } ValType;

typedef struct Obj Obj;
typedef struct Str Str;
typedef struct List List;
typedef struct Func Func;
typedef struct Env Env;

typedef struct {
    ValType type;
    union {
        bool b;
        double n;
        Str *s;
        List *l;
        Func *f;
        Env *e;
        int builtin;
    } as;
} Value;

struct Obj {
    ValType type;
    bool marked;
    Obj *next;
};

struct Str {
    Obj obj;
    size_t len;
    char data[]; /* UTF-8, dipungkasi \0 */
};

struct List {
    Obj obj;
    size_t len, cap;
    Value *items;
};

struct Func {
    Obj obj;
    Node *node;
    Env *closure;
};

struct Env {
    Obj obj;
    Env *parent;
    int len, cap;
    const char **names;
    Value *vals;
};

struct Node {
    NodeKind kind;
    int line;
    OpKind op;
    const char *name;
    Node *a, *b;
    NodeList body; /* blok, argumen, utawa isi daftar */
    NodeList alt;  /* blok liyane */
    bool has_alt;
    NameList params;
    Value lit;
};

typedef struct {
    NodeList body;
} Program;

Program parse(const char *src);

/* ---------- obyek lan GC ---------- */

#define NULL_VAL ((Value){V_NULL, {.b = false}})
static inline Value bool_val(bool b) { Value v = {V_BOOL, {.b = b}}; return v; }
static inline Value num_val(double n) { Value v = {V_NUM, {.n = n}}; return v; }

Str *str_new(const char *s, size_t n);
Str *str_new_permanent(const char *s, size_t n);
List *list_new(size_t cap);
void list_push(List *l, Value v);
Func *func_new(Node *node, Env *closure);
Env *env_new(Env *parent);

Value str_val(Str *s);
Value list_val(List *l);

void env_define(Env *e, const char *name, Value v);
Env *env_find(Env *e, const char *name, int *index);

/* Tumpukan root: nilai sing kudu tetep urip nalika GC mlaku. */
void root_push(Value v);
void root_pop(int n);
int root_mark(void);
void root_reset(int mark);
void gc_set_globals(Env *globals);
void gc_maybe_collect(void);

/* ---------- format ---------- */

size_t utf8_count(const char *s, size_t n);
size_t utf8_seq_len(unsigned char c);
void number_text(Buf *b, double x);
void format_value(Buf *b, Value v, bool nested);
const char *type_name(Value v);
bool values_equal(Value a, Value b);

/* ---------- interpreter ---------- */

typedef struct Interp Interp;
Interp *interp_new(void);
/* Nglakokake kode; ngasilake nilai ukara pungkasan (kanggo REPL). */
Value interp_run(Interp *in, const char *src);
/* Mbalekake kahanan interpreter sawise ana kesalahan (kanggo REPL). */
void interp_reset(Interp *in);
/* Watesan tumpukan C: yen rekursi ngluwihi `budget` bait saka `base`,
 * interpreter menehi kesalahan "rekursi kakehan" tinimbang crash. */
void interp_set_stack(Interp *in, const char *base, size_t budget);

#endif
