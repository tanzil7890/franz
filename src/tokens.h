#ifndef TOKENS_H
#define TOKENS_H

enum TokenType {
  TOK_ASSIGNMENT,
  TOK_APPLYOPEN,
  TOK_APPLYCLOSE,
  TOK_FUNCOPEN,
  TOK_FUNCCLOSE,
  TOK_LBRACKET,   // '[' for list literals ()
  TOK_RBRACKET,   // ']' for list literals ()
  TOK_COMMA,
  TOK_ARROW,
  TOK_RETURN,
  TOK_IDENTIFIER,
  TOK_INT,
  TOK_FLOAT,
  TOK_STRING,
  TOK_SIG,        // Type signature token
  TOK_AS,         // 'as' keyword for namespace aliasing
  TOK_DOT,        // '.' for qualified names (ns.identifier)
  TOK_MUT,        // 'mut' keyword for mutable variable declarations
  TOK_END,
  TOK_START
};

//  Industry-standard array-based token 
// Individual token element (no p_next pointer - stored in array)
typedef struct Token {
  char *val;
  enum TokenType type;
  int lineNumber;
} Token;

//  Dynamic array container for tokens (industry standard)
// Replaces linked list with O(1) append and O(1) random access
typedef struct TokenArray {
  Token *tokens;        // Dynamic array of tokens
  int count;            // Number of tokens currently stored
  int capacity;         // Allocated capacity (grows as needed)
} TokenArray;

// prototypes
char* getTokenTypeString(enum TokenType);

//  Array-based token operations
TokenArray* TokenArray_new(void);
void TokenArray_push(TokenArray *arr, char *val, enum TokenType type, int lineNumber);
void TokenArray_print(TokenArray *arr);
void TokenArray_free(TokenArray *arr);

#endif