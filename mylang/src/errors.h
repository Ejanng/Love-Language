#pragma once
// C has no exceptions, so errors use setjmp/longjmp. A phase that can fail calls setjmp(g_err.env)
// first; lang_error() fills in g_err and jumps back there. Every error carries the source line
// so messages can point at the problem.
#include <setjmp.h>

typedef enum {
    ERR_SYNTAX,   // raised by the lexer and parser
    ERR_COMPILE,  // raised by the bytecode compiler for programs that parse but can't be compiled
    ERR_RUNTIME,  // raised while a program is running
} ErrorKind;

typedef struct {
    jmp_buf env;
    ErrorKind kind;
    int line;
    char message[512];
} ErrorState;

extern ErrorState g_err;

#if defined(__GNUC__)
#define PRINTF_LIKE(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define PRINTF_LIKE(fmt, args)
#endif

_Noreturn void lang_error(ErrorKind kind, int line, const char* fmt, ...) PRINTF_LIKE(3, 4);

// Prints "[line N] Syntax error: message" (or Compile/Runtime error) for the error in g_err.
// Flushes stdout first so program output stays ahead of the error when both go to one stream.
void report_error(void);

// Process exit codes shared by both backends.
enum { EXIT_OK = 0, EXIT_USAGE = 64, EXIT_COMPILE_ERROR = 65, EXIT_RUNTIME_ERROR = 70, EXIT_NO_FILE = 74 };
