#pragma once
// Stage 1: the lexer turns source text into a flat list of tokens.
// The list is stored in the arena and ends with a TOK_EOF token. Throws ERR_SYNTAX on bad input.
#include <stddef.h>

#include "token.h"

Token* lex(const char* source, size_t length, int* count);
