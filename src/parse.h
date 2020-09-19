#ifndef PARSE_H
#define PARSE_H
#include "tokens.h"
#include "ast.h"

//  Array-based parsing prototypes
int skipClosure(TokenArray *arr, int start, enum TokenType open, enum TokenType close);

AstNode *parseValue(TokenArray *arr, int start, int length);
AstNode *parseAssignment(TokenArray *arr, int start, int length);
AstNode *parseStatement(TokenArray *arr, int start, int length);
AstNode *parseApplication(TokenArray *arr, int start, int length);
AstNode *parseReturn(TokenArray *arr, int start, int length);
AstNode *parseFunction(TokenArray *arr, int start, int length);
AstNode *parseProgram(TokenArray *arr);

#endif