#ifndef LEX_H
#define LEX_H
#include "tokens.h"

//  Array-based lexer prototype
TokenArray* lex(char *code, int fileLength);

#endif