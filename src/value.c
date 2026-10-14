/* Nilai, obyek, pangresikan memori (GC), lan format. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jws.h"

/* ---------- kesalahan ---------- */

jmp_buf *jws_error_jmp;
char *jws_error_msg;

static void buf_vprintf(Buf *b, const char *fmt, va_list ap);

void jws_error(int line, const char *fmt, ...) {
    Buf b = {0};
    if (line > 0) buf_printf(&b, "Kesalahan ing baris %d: ", line);
    else buf_puts(&b, "Kesalahan: ");
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(&b, fmt, ap);
    va_end(ap);
    free(jws_error_msg);
    jws_error_msg = buf_take(&b);
    if (jws_error_jmp) longjmp(*jws_error_jmp, 1);
    fprintf(stderr, "%s\n", jws_error_msg);
    exit(1);
}

static void *xrealloc(void *p, size_t n) {
    p = realloc(p, n);
    if (!p && n) {
        fprintf(stderr, "Kesalahan: memori entek\n");
        exit(1);
    }
    return p;
}

/* ---------- buffer ---------- */

static void buf_reserve(Buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->len + extra + 1) cap *= 2;
    b->data = xrealloc(b->data, cap);
    b->cap = cap;
}

void buf_push(Buf *b, char c) {
    buf_reserve(b, 1);
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
}

void buf_append(Buf *b, const char *s, size_t n) {
    buf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void buf_puts(Buf *b, const char *s) { buf_append(b, s, strlen(s)); }

static void buf_vprintf(Buf *b, const char *fmt, va_list ap) {
    char small[256];
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(small, sizeof small, fmt, ap);
    if ((size_t)n < sizeof small) {
        buf_append(b, small, (size_t)n);
    } else {
        buf_reserve(b, (size_t)n);
        vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap2);
        b->len += (size_t)n;
    }
    va_end(ap2);
}

void buf_printf(Buf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(b, fmt, ap);
    va_end(ap);
}

char *buf_take(Buf *b) {
    if (!b->data) buf_reserve(b, 0), b->data[0] = '\0';
    char *s = b->data;
    *b = (Buf){0};
    return s;
}

/* ---------- obyek ---------- */

static Obj *all_objects;
static size_t bytes_allocated;
static size_t next_gc = 1 << 20;

static void *obj_alloc(size_t size, ValType type) {
    Obj *o = xrealloc(NULL, size);
    o->type = type;
    o->marked = false;
    o->next = all_objects;
    all_objects = o;
    bytes_allocated += size;
    return o;
}

Str *str_new(const char *s, size_t n) {
    Str *str = obj_alloc(sizeof(Str) + n + 1, V_STR);
    str->len = n;
    memcpy(str->data, s, n);
    str->data[n] = '\0';
    return str;
}

/* Tembung literal saka kode: ora tau dibuang dening GC. */
Str *str_new_permanent(const char *s, size_t n) {
    Str *str = xrealloc(NULL, sizeof(Str) + n + 1);
    str->obj.type = V_STR;
    str->obj.marked = true;
    str->obj.next = NULL;
    str->len = n;
    memcpy(str->data, s, n);
    str->data[n] = '\0';
    return str;
}

List *list_new(size_t cap) {
    List *l = obj_alloc(sizeof(List), V_LIST);
    l->len = 0;
    l->cap = cap;
    l->items = cap ? xrealloc(NULL, sizeof(Value) * cap) : NULL;
    bytes_allocated += sizeof(Value) * cap;
    return l;
}

void list_push(List *l, Value v) {
    if (l->len == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 4;
        l->items = xrealloc(l->items, sizeof(Value) * cap);
        bytes_allocated += sizeof(Value) * (cap - l->cap);
        l->cap = cap;
    }
    l->items[l->len++] = v;
}

Func *func_new(Node *node, Env *closure) {
    Func *f = obj_alloc(sizeof(Func), V_FUNC);
    f->node = node;
    f->closure = closure;
    return f;
}

Env *env_new(Env *parent) {
    Env *e = obj_alloc(sizeof(Env), V_ENV);
    e->parent = parent;
    e->len = e->cap = 0;
    e->names = NULL;
    e->vals = NULL;
    return e;
}

Value str_val(Str *s) {
    Value v = {V_STR, {.s = s}};
    return v;
}

Value list_val(List *l) {
    Value v = {V_LIST, {.l = l}};
    return v;
}

/* Jeneng wis di-intern, mula cukup mbandhingake pointer. */
void env_define(Env *e, const char *name, Value v) {
    for (int i = 0; i < e->len; i++) {
        if (e->names[i] == name) {
            e->vals[i] = v;
            return;
        }
    }
    if (e->len == e->cap) {
        e->cap = e->cap ? e->cap * 2 : 4;
        e->names = xrealloc(e->names, sizeof(char *) * e->cap);
        e->vals = xrealloc(e->vals, sizeof(Value) * e->cap);
        bytes_allocated += (sizeof(char *) + sizeof(Value)) * 4;
    }
    e->names[e->len] = name;
    e->vals[e->len] = v;
    e->len++;
}

Env *env_find(Env *e, const char *name, int *index) {
    for (; e; e = e->parent) {
        for (int i = 0; i < e->len; i++) {
            if (e->names[i] == name) {
                *index = i;
                return e;
            }
        }
    }
    return NULL;
}

/* ---------- GC: mark & sweep ----------
 * GC mung mlaku ing wiwitan ukara (gc_maybe_collect). Nilai sementara sing
 * kudu tetep urip nalika ngevaluasi ekspresi liyane disimpen ing tumpukan
 * root. */

static Value *roots;
static int roots_len, roots_cap;
static Env *globals_env;

void root_push(Value v) {
    if (roots_len == roots_cap) {
        roots_cap = roots_cap ? roots_cap * 2 : 256;
        roots = xrealloc(roots, sizeof(Value) * roots_cap);
    }
    roots[roots_len++] = v;
}

void root_pop(int n) { roots_len -= n; }
int root_mark(void) { return roots_len; }
void root_reset(int mark) { roots_len = mark; }
void gc_set_globals(Env *globals) { globals_env = globals; }

static Obj **gray;
static size_t gray_len, gray_cap;

static void mark_obj(Obj *o) {
    if (!o || o->marked) return;
    o->marked = true;
    if (gray_len == gray_cap) {
        gray_cap = gray_cap ? gray_cap * 2 : 256;
        gray = xrealloc(gray, sizeof(Obj *) * gray_cap);
    }
    gray[gray_len++] = o;
}

static void mark_value(Value v) {
    switch (v.type) {
    case V_STR: mark_obj(&v.as.s->obj); break;
    case V_LIST: mark_obj(&v.as.l->obj); break;
    case V_FUNC: mark_obj(&v.as.f->obj); break;
    case V_ENV: mark_obj(&v.as.e->obj); break;
    default: break;
    }
}

static void trace(Obj *o) {
    switch (o->type) {
    case V_LIST: {
        List *l = (List *)o;
        for (size_t i = 0; i < l->len; i++) mark_value(l->items[i]);
        break;
    }
    case V_FUNC: mark_obj(&((Func *)o)->closure->obj); break;
    case V_ENV: {
        Env *e = (Env *)o;
        if (e->parent) mark_obj(&e->parent->obj);
        for (int i = 0; i < e->len; i++) mark_value(e->vals[i]);
        break;
    }
    default: break;
    }
}

static size_t obj_size(Obj *o) {
    switch (o->type) {
    case V_STR: return sizeof(Str) + ((Str *)o)->len + 1;
    case V_LIST: return sizeof(List) + sizeof(Value) * ((List *)o)->cap;
    case V_FUNC: return sizeof(Func);
    case V_ENV: return sizeof(Env) + (sizeof(char *) + sizeof(Value)) * (size_t)((Env *)o)->cap;
    default: return 0;
    }
}

static void free_obj(Obj *o) {
    if (o->type == V_LIST) free(((List *)o)->items);
    if (o->type == V_ENV) {
        free(((Env *)o)->names);
        free(((Env *)o)->vals);
    }
    free(o);
}

static void collect(void) {
    if (globals_env) mark_obj(&globals_env->obj);
    for (int i = 0; i < roots_len; i++) mark_value(roots[i]);
    while (gray_len) trace(gray[--gray_len]);

    size_t live = 0;
    Obj **link = &all_objects;
    while (*link) {
        Obj *o = *link;
        if (o->marked) {
            o->marked = false;
            live += obj_size(o);
            link = &o->next;
        } else {
            *link = o->next;
            free_obj(o);
        }
    }
    bytes_allocated = live;
    next_gc = live * 2 > (1 << 20) ? live * 2 : (1 << 20);
}

void gc_maybe_collect(void) {
#ifdef JWS_GC_STRESS
    collect();
#else
    if (bytes_allocated > next_gc) collect();
#endif
}

/* ---------- UTF-8 ---------- */

size_t utf8_seq_len(unsigned char c) {
    if (c < 0x80) return 1;
    if (c >= 0xF0) return 4;
    if (c >= 0xE0) return 3;
    if (c >= 0xC0) return 2;
    return 1;
}

/* Cacahe karakter (code point), dudu cacahe bait. */
size_t utf8_count(const char *s, size_t n) {
    size_t count = 0;
    for (size_t i = 0; i < n;) {
        size_t len = utf8_seq_len((unsigned char)s[i]);
        i += len;
        count++;
    }
    return count;
}

/* ---------- format angka ----------
 * Digit paling cendhak sing isih persis; angka gedhe banget (>= 1e21) utawa
 * cilik banget (< 1e-6) ditulis nganggo eksponen, contone 1e+21. */

void number_text(Buf *b, double x) {
    if (isnan(x)) {
        buf_puts(b, "NaN");
        return;
    }
    if (x == 0) {
        buf_push(b, '0');
        return;
    }
    if (x < 0) {
        buf_push(b, '-');
        x = -x;
    }
    if (isinf(x)) {
        buf_puts(b, "Infinity");
        return;
    }

    /* Golek digit paling cendhak sing isih bali menyang angka sing padha. */
    char tmp[64];
    for (int prec = 0; prec < 17; prec++) {
        snprintf(tmp, sizeof tmp, "%.*e", prec, x);
        if (strtod(tmp, NULL) == x) break;
    }
    char digits[32];
    int k = 0;
    char *p = tmp;
    for (; *p && *p != 'e'; p++) {
        if (*p >= '0' && *p <= '9') digits[k++] = *p;
    }
    int exp10 = atoi(p + 1);
    while (k > 1 && digits[k - 1] == '0') k--;
    int n = exp10 + 1;

    if (k <= n && n <= 21) {
        buf_append(b, digits, (size_t)k);
        for (int i = 0; i < n - k; i++) buf_push(b, '0');
    } else if (0 < n && n <= 21) {
        buf_append(b, digits, (size_t)n);
        buf_push(b, '.');
        buf_append(b, digits + n, (size_t)(k - n));
    } else if (-6 < n && n <= 0) {
        buf_puts(b, "0.");
        for (int i = 0; i < -n; i++) buf_push(b, '0');
        buf_append(b, digits, (size_t)k);
    } else {
        buf_push(b, digits[0]);
        if (k > 1) {
            buf_push(b, '.');
            buf_append(b, digits + 1, (size_t)(k - 1));
        }
        buf_printf(b, "e%c%d", n - 1 < 0 ? '-' : '+', abs(n - 1));
    }
}

/* Angka dudu wilangan bunder dibunderake nganti 10 digit sawise koma. */
static void format_number(Buf *b, double x) {
    if (isfinite(x) && x != floor(x)) {
        char tmp[400];
        snprintf(tmp, sizeof tmp, "%.10f", x);
        x = strtod(tmp, NULL);
    }
    number_text(b, x);
}

static void format_quoted(Buf *b, Str *s) {
    buf_push(b, '"');
    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)s->data[i];
        switch (c) {
        case '"': buf_puts(b, "\\\""); break;
        case '\\': buf_puts(b, "\\\\"); break;
        case '\n': buf_puts(b, "\\n"); break;
        case '\r': buf_puts(b, "\\r"); break;
        case '\t': buf_puts(b, "\\t"); break;
        case '\b': buf_puts(b, "\\b"); break;
        case '\f': buf_puts(b, "\\f"); break;
        default:
            if (c < 0x20) buf_printf(b, "\\u%04x", c);
            else buf_push(b, (char)c);
        }
    }
    buf_push(b, '"');
}

extern const char *builtin_name(int index);

void format_value(Buf *b, Value v, bool nested) {
    switch (v.type) {
    case V_NULL: buf_puts(b, "kosong"); break;
    case V_BOOL: buf_puts(b, v.as.b ? "bener" : "luput"); break;
    case V_NUM: format_number(b, v.as.n); break;
    case V_STR:
        if (nested) format_quoted(b, v.as.s);
        else buf_append(b, v.as.s->data, v.as.s->len);
        break;
    case V_LIST:
        buf_push(b, '[');
        for (size_t i = 0; i < v.as.l->len; i++) {
            if (i) buf_puts(b, ", ");
            format_value(b, v.as.l->items[i], true);
        }
        buf_push(b, ']');
        break;
    case V_FUNC:
        if (v.as.f->node->name) buf_printf(b, "<gawe %s>", v.as.f->node->name);
        else buf_puts(b, "<gawe>");
        break;
    case V_BUILTIN: buf_printf(b, "<gawe %s>", builtin_name(v.as.builtin)); break;
    case V_ENV: buf_puts(b, "<env>"); break;
    }
}

const char *type_name(Value v) {
    switch (v.type) {
    case V_NULL: return "kosong";
    case V_BOOL: return "logika";
    case V_NUM: return "angka";
    case V_STR: return "tembung";
    case V_LIST: return "daftar";
    default: return "gawe";
    }
}

bool values_equal(Value a, Value b) {
    if (a.type == V_LIST && b.type == V_LIST) {
        if (a.as.l->len != b.as.l->len) return false;
        for (size_t i = 0; i < a.as.l->len; i++) {
            if (!values_equal(a.as.l->items[i], b.as.l->items[i])) return false;
        }
        return true;
    }
    if (a.type != b.type) return false;
    switch (a.type) {
    case V_NULL: return true;
    case V_BOOL: return a.as.b == b.as.b;
    case V_NUM: return a.as.n == b.as.n;
    case V_STR: return a.as.s->len == b.as.s->len && memcmp(a.as.s->data, b.as.s->data, a.as.s->len) == 0;
    case V_FUNC: return a.as.f == b.as.f;
    case V_BUILTIN: return a.as.builtin == b.as.builtin;
    default: return false;
    }
}
