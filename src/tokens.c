#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"

#define INITIAL_TOKEN_CAPACITY 16  // Start with 16 tokens, double as needed

// get token type as string, given enum
char* getTokenTypeString(enum TokenType type) {
  switch (type) {
    case TOK_ASSIGNMENT: return "assignment";
    case TOK_APPLYOPEN: return "applyopen";
    case TOK_APPLYCLOSE: return "applyclose";
    case TOK_FUNCOPEN: return "funcopen";
    case TOK_FUNCCLOSE: return "funcclose";
    case TOK_LBRACKET: return "lbracket";
    case TOK_RBRACKET: return "rbracket";
    case TOK_COMMA: return "comma";
    case TOK_ARROW: return "arrow";
    case TOK_RETURN: return "return";
    case TOK_IDENTIFIER: return "identifier";
    case TOK_INT: return "int";
    case TOK_FLOAT: return "float";
    case TOK_STRING: return "string";
    case TOK_SIG: return "sig";
    case TOK_AS: return "as";
    case TOK_DOT: return "dot";
    case TOK_MUT: return "mut";
    case TOK_START: return "start";
    case TOK_END: return "end";
    default: return "unknown";
  }
}

//  Create new TokenArray with initial capacity
TokenArray* TokenArray_new(void) {
  TokenArray *arr = (TokenArray *)malloc(sizeof(TokenArray));
  if (arr == NULL) {
    fprintf(stderr, "Error: Failed to allocate TokenArray\n");
    exit(1);
  }

  arr->capacity = INITIAL_TOKEN_CAPACITY;
  arr->count = 0;
  arr->tokens = (Token *)malloc(sizeof(Token) * arr->capacity);

  if (arr->tokens == NULL) {
    fprintf(stderr, "Error: Failed to allocate token array\n");
    free(arr);
    exit(1);
  }

  return arr;
}

//  Add token to array (O(1) amortized)
void TokenArray_push(TokenArray *arr, char *val, enum TokenType type, int lineNumber) {
  if (arr == NULL) {
    fprintf(stderr, "Error: TokenArray is NULL\n");
    exit(1);
  }

  // Grow array if needed (double capacity)
  if (arr->count >= arr->capacity) {
    int newCapacity = arr->capacity * 2;
    Token *newTokens = (Token *)realloc(arr->tokens, sizeof(Token) * newCapacity);

    if (newTokens == NULL) {
      fprintf(stderr, "Error: Failed to grow token array\n");
      exit(1);
    }

    arr->tokens = newTokens;
    arr->capacity = newCapacity;
  }

  // Add token at end (O(1))
  arr->tokens[arr->count].val = val;
  arr->tokens[arr->count].type = type;
  arr->tokens[arr->count].lineNumber = lineNumber;
  arr->count++;
}

//  Print token array
void TokenArray_print(TokenArray *arr) {
  if (arr == NULL) {
    printf("TokenArray is NULL\n");
    return;
  }

  for (int i = 0; i < arr->count; i++) {
    Token *tok = &arr->tokens[i];
    if (tok->val == NULL) {
      printf("%i| %s\n", tok->lineNumber, getTokenTypeString(tok->type));
    } else {
      printf("%i| %s: %s\n", tok->lineNumber, getTokenTypeString(tok->type), tok->val);
    }
  }
}

//  Free token array and all token values
void TokenArray_free(TokenArray *arr) {
  if (arr == NULL) return;

  // Free all token values
  for (int i = 0; i < arr->count; i++) {
    free(arr->tokens[i].val);
  }

  // Free token array
  free(arr->tokens);

  // Free container
  free(arr);
}