/* jws: nglakokake berkas JavaneseScript utawa mbukak REPL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jws.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = {0};
    char chunk[8192];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) buf_append(&b, chunk, n);
    fclose(f);
    return buf_take(&b);
}

/* Ngitung kurung sing durung ditutup, kanggo input pirang-pirang baris. */
static int bracket_depth(const char *s) {
    int depth = 0;
    for (; *s; s++) {
        if (strchr("{[(", *s)) depth++;
        else if (strchr("}])", *s)) depth--;
    }
    return depth;
}

static void repl(Interp *in) {
    printf("JavaneseScript - sugeng rawuh! (Ctrl+D kanggo metu)\n");
    Buf input = {0};
    char line[4096];
    for (;;) {
        fputs(input.len ? "...  " : "jws> ", stdout);
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        buf_puts(&input, line);
        if (line[strlen(line) - 1] != '\n') buf_push(&input, '\n');
        if (bracket_depth(input.data) > 0) continue;

        jmp_buf jb;
        jws_error_jmp = &jb;
        if (setjmp(jb) == 0) {
            Value result = interp_run(in, input.data);
            fflush(stdout);
            if (result.type != V_NULL) {
                Buf out = {0};
                format_value(&out, result, true);
                printf("%s\n", out.data);
                free(out.data);
            }
        } else {
            fflush(stdout);
            fprintf(stderr, "%s\n", jws_error_msg);
            interp_reset(in);
        }
        jws_error_jmp = NULL;
        input.len = 0;
        input.data[0] = '\0';
    }
    printf("\nMatur nuwun!\n");
    free(input.data);
}

typedef struct {
    int argc;
    char **argv;
    int status;
} MainArgs;

static int run_main(int argc, char **argv, size_t stack_size);

#ifndef _WIN32
#include <pthread.h>

/* Interpreter mlaku ing thread kanthi tumpukan gedhe supaya rekursi jero
 * ora gampang kentekan tumpukan. Memori mung dienggo yen pancen perlu. */
#define BIG_STACK ((size_t)512 * 1024 * 1024)

static void *thread_main(void *p) {
    MainArgs *a = p;
    a->status = run_main(a->argc, a->argv, BIG_STACK);
    return NULL;
}

int main(int argc, char **argv) {
    MainArgs a = {argc, argv, 0};
    pthread_attr_t attr;
    pthread_t th;
    pthread_attr_init(&attr);
    if (pthread_attr_setstacksize(&attr, BIG_STACK) == 0 && pthread_create(&th, &attr, thread_main, &a) == 0) {
        pthread_join(th, NULL);
        return a.status;
    }
    return run_main(argc, argv, 8 * 1024 * 1024);
}
#else
int main(int argc, char **argv) { return run_main(argc, argv, 1024 * 1024); }
#endif

static int run_main(int argc, char **argv, size_t stack_size) {
    char stack_base;
    const char *file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Panganggon: jws [berkas.jws]\nTanpa berkas: mbukak REPL.\n");
            return 0;
        }
        if (argv[i][0] != '-' && !file) file = argv[i];
    }

    Interp *in = interp_new();
    /* Sisakake 1/4 tumpukan kanggo GC, format, lan pangurai. */
    interp_set_stack(in, &stack_base, stack_size / 4 * 3);
    if (!file) {
        repl(in);
        return 0;
    }

    char *src = read_file(file);
    if (!src) {
        fprintf(stderr, "Kesalahan: berkas \"%s\" ora bisa dibukak\n", file);
        return 1;
    }
    jmp_buf jb;
    jws_error_jmp = &jb;
    if (setjmp(jb) != 0) {
        fflush(stdout);
        fprintf(stderr, "%s\n", jws_error_msg);
        return 1;
    }
    interp_run(in, src);
    fflush(stdout);
    return 0;
}
