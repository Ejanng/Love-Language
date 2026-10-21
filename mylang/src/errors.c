#include "errors.h"

#include <stdarg.h>
#include <stdio.h>

ErrorState g_err;

void lang_error(ErrorKind kind, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_err.message, sizeof g_err.message, fmt, args);
    va_end(args);
    g_err.kind = kind;
    g_err.line = line;
    longjmp(g_err.env, 1);
}

void report_error(void) {
    const char* kind = g_err.kind == ERR_SYNTAX ? "Syntax error" : g_err.kind == ERR_COMPILE ? "Compile error" : "Runtime error";
    fflush(stdout);
    fprintf(stderr, "[line %d] %s: %s\n", g_err.line, kind, g_err.message);
}
