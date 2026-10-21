// Entry point: `mylang script.rene` runs a file, `mylang` alone starts a REPL.
// `--vm` runs programs on the bytecode VM instead of the tree-walking interpreter,
// and `--dump` (which implies --vm) also prints the compiled bytecode before running it.
#define _POSIX_C_SOURCE 200809L  // for getline

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "errors.h"
#include "interpreter.h"
#include "value.h"
#include "vm/vm.h"

// The selected backend: runs a piece of source and returns an exit code.
typedef struct {
    bool useVm;
    Interpreter interpreter;
    VM vm;
} Runner;

static int run_source(Runner* r, const char* source, size_t length) {
    return r->useVm ? vm_run(&r->vm, source, length) : interpreter_run(&r->interpreter, source, length);
}

static int run_file(Runner* r, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", path);
        return EXIT_NO_FILE;
    }
    size_t cap = 4096, len = 0;
    char* buffer = malloc(cap);
    size_t n;
    while (buffer && (n = fread(buffer + len, 1, cap - len, f)) > 0) {
        len += n;
        if (len == cap) buffer = realloc(buffer, cap *= 2);
    }
    fclose(f);
    if (!buffer) {
        fputs("Out of memory.\n", stderr);
        return EXIT_FAILURE;
    }
    int result = run_source(r, buffer, len);
    free(buffer);
    return result;
}

static void repl(Runner* r) {
    printf("mylang REPL. Type 'exit' to quit.\n");
    char* line = NULL;
    size_t cap = 0;
    for (;;) {
        printf("> ");
        fflush(stdout);
        ssize_t got = getline(&line, &cap, stdin);
        if (got < 0) break;
        size_t len = (size_t)got;
        while (len > 0 && strchr(" \t\r\n", line[len - 1])) len--;
        if (len == 0) continue;
        line[len] = '\0';
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        // Convenience: allow typing `print 1 + 2` without the final semicolon.
        if (line[len - 1] != ';' && line[len - 1] != '}') {
            if (len + 2 > cap) line = realloc(line, cap = len + 2);
            line[len++] = ';';
            line[len] = '\0';
        }
        run_source(r, line, len);  // errors are printed; the session keeps going
    }
    free(line);
}

int main(int argc, char** argv) {
    bool useVm = false;
    bool dump = false;
    const char* path = NULL;
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "--vm") == 0) {
            useVm = true;
        } else if (strcmp(arg, "--dump") == 0) {
            useVm = dump = true;
        } else if (!path && strncmp(arg, "--", 2) != 0) {
            path = arg;
        } else {
            fputs("Usage: mylang [--vm] [--dump] [script.rene]\n", stderr);
            return EXIT_USAGE;
        }
    }

    Runner runner = {.useVm = useVm};
    interpreter_init(&runner.interpreter);
    vm_init(&runner.vm, dump);

    int result = EXIT_OK;
    if (path) {
        result = run_file(&runner, path);
    } else {
        repl(&runner);
    }

    interpreter_free(&runner.interpreter);
    vm_free(&runner.vm);
    objects_free_all();
    arena_free_all();
    return result;
}
