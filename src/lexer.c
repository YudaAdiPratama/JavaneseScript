/* Lexer: mecah kode dadi token. */
#include <stdlib.h>
#include <string.h>

#include "jws.h"

static const char *KEYWORDS[] = {
    "simpen",  /* deklarasi variabel */
    "tulis",   /* cetak */
    "yen",     /* if */
    "liyane",  /* else */
    "salawase", /* while */
    "saben",   /* for each */
    "ing",     /* in */
    "gawe",    /* fungsi */
    "balekna", /* return */
    "mandheg", /* break */
    "terus",   /* continue */
    "bener",   /* true */
    "luput",   /* false */
    "kosong",  /* null */
    "lan",     /* and */
    "utawa",   /* or */
    "ora",     /* not */
    NULL,
};

/* ---------- intern: saben jeneng mung disimpen sepisan ---------- */

typedef struct InternEntry {
    char *s;
    size_t n;
    struct InternEntry *next;
} InternEntry;

#define INTERN_BUCKETS 1024
static InternEntry *intern_table[INTERN_BUCKETS];

const char *intern(const char *s, size_t n) {
    unsigned long h = 5381;
    for (size_t i = 0; i < n; i++) h = h * 33 + (unsigned char)s[i];
    InternEntry **slot = &intern_table[h % INTERN_BUCKETS];
    for (InternEntry *e = *slot; e; e = e->next) {
        if (e->n == n && memcmp(e->s, s, n) == 0) return e->s;
    }
    InternEntry *e = malloc(sizeof *e);
    e->s = malloc(n + 1);
    memcpy(e->s, s, n);
    e->s[n] = '\0';
    e->n = n;
    e->next = *slot;
    *slot = e;
    return e->s;
}

/* ---------- lexer ---------- */

static bool is_ident_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_ident_part(char c) { return is_ident_start(c) || is_digit(c); }
static bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f'; }

static void push(TokenList *tl, Token t) {
    if (tl->len == tl->cap) {
        tl->cap = tl->cap ? tl->cap * 2 : 64;
        tl->items = realloc(tl->items, sizeof(Token) * tl->cap);
    }
    tl->items[tl->len++] = t;
}

static bool is_keyword(const char *s) {
    for (int i = 0; KEYWORDS[i]; i++) {
        if (strcmp(KEYWORDS[i], s) == 0) return true;
    }
    return false;
}

TokenList tokenize(const char *src) {
    TokenList tl = {0};
    size_t n = strlen(src);
    size_t i = 0;
    int line = 1;

    /* Lewati BOM UTF-8 */
    if (n >= 3 && (unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) i = 3;

    while (i < n) {
        char c = src[i];

        if (c == '\n') {
            line++;
            i++;
            continue;
        }
        if (is_space(c)) {
            i++;
            continue;
        }

        /* Komentar: // nganti pungkasan baris */
        if (c == '/' && src[i + 1] == '/') {
            while (i < n && src[i] != '\n') i++;
            continue;
        }

        /* Titik koma minangka pamisah ukara (opsional). */
        if (c == ';') {
            push(&tl, (Token){T_END, line, intern(";", 1), 1, 0});
            i++;
            continue;
        }

        if (is_digit(c)) {
            size_t start = i;
            while (i < n && is_digit(src[i])) i++;
            if (src[i] == '.' && i + 1 < n && is_digit(src[i + 1])) {
                i++;
                while (i < n && is_digit(src[i])) i++;
            }
            const char *text = intern(src + start, i - start);
            push(&tl, (Token){T_NUM, line, text, i - start, strtod(text, NULL)});
            continue;
        }

        if (c == '"') {
            int start_line = line;
            Buf b = {0};
            i++;
            while (i < n && src[i] != '"') {
                char ch = src[i];
                if (ch == '\n') line++;
                if (ch == '\\') {
                    i++;
                    char esc = i < n ? src[i] : '\0';
                    switch (esc) {
                    case 'n': ch = '\n'; break;
                    case 't': ch = '\t'; break;
                    case '"': ch = '"'; break;
                    case '\\': ch = '\\'; break;
                    default: {
                        size_t len = esc ? utf8_seq_len((unsigned char)esc) : 0;
                        if (i + len > n) len = n - i;
                        free(b.data);
                        jws_error(line, "escape ora dikenal: \\%.*s", (int)len, src + i);
                    }
                    }
                }
                buf_push(&b, ch);
                i++;
            }
            if (i >= n) {
                free(b.data);
                jws_error(start_line, "tembung (string) ora ditutup");
            }
            i++;
            size_t len = b.len;
            char *text = buf_take(&b);
            push(&tl, (Token){T_STR, start_line, text, len, 0});
            continue;
        }

        if (is_ident_start(c)) {
            size_t start = i;
            while (i < n && is_ident_part(src[i])) i++;
            const char *text = intern(src + start, i - start);
            push(&tl, (Token){is_keyword(text) ? T_KW : T_IDENT, line, text, i - start, 0});
            continue;
        }

        static const char *TWO[] = {"==", "!=", "<=", ">=", NULL};
        bool matched = false;
        for (int k = 0; TWO[k]; k++) {
            if (i + 1 < n && src[i] == TWO[k][0] && src[i + 1] == TWO[k][1]) {
                push(&tl, (Token){T_OP, line, intern(TWO[k], 2), 2, 0});
                i += 2;
                matched = true;
                break;
            }
        }
        if (matched) continue;

        if (strchr("+-*/%<>=(){}[],", c)) {
            push(&tl, (Token){T_OP, line, intern(&src[i], 1), 1, 0});
            i++;
            continue;
        }

        size_t len = utf8_seq_len((unsigned char)c);
        if (i + len > n) len = n - i;
        jws_error(line, "karakter ora dikenal: \"%.*s\"", (int)len, src + i);
    }

    push(&tl, (Token){T_EOF, line, NULL, 0, 0});
    return tl;
}
