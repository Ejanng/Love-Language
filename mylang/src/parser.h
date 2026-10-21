#pragma once
// Stage 2: a recursive descent parser turns tokens into an AST.
//
// Grammar (lowest precedence first):
//   program     -> declaration* EOF
//   declaration -> fnDecl | letDecl | statement
//   fnDecl      -> "fn" IDENT "(" params? ")" block
//   letDecl     -> "let" IDENT ( "=" expression )? ";"
//   statement   -> print | if | while | return | block | exprStmt
//   expression  -> assignment
//   assignment  -> IDENT "=" assignment | logicOr
//   logicOr     -> logicAnd ( "or" logicAnd )*
//   logicAnd    -> equality ( "and" equality )*
//   equality    -> comparison ( ( "!=" | "==" ) comparison )*
//   comparison  -> term ( ( ">" | ">=" | "<" | "<=" ) term )*
//   term        -> factor ( ( "-" | "+" ) factor )*
//   factor      -> unary ( ( "/" | "*" | "%" ) unary )*
//   unary       -> ( "!" | "-" ) unary | call
//   call        -> primary ( "(" arguments? ")" )*
//   primary     -> NUMBER | STRING | "true" | "false" | "nil" | IDENT | "(" expression ")"
#include "ast.h"

// Throws ERR_SYNTAX on bad input. The tree is allocated in the arena.
Program* parse(const Token* tokens, int count);
