#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include "ast.h"
#include "tokens.h"

//  Skip closure using array indexing
// Returns the index of the closing token
int skipClosure(TokenArray *arr, int start, enum TokenType open, enum TokenType close) {
  int depth = 1;
  int i = start + 1;

  while (depth != 0 && i < arr->count) {
    Token *tok = &arr->tokens[i];

    if (tok->type == TOK_END) {
      printf(
        "Syntax Error @ Line %i: Unexpected %s token.\n",
        tok->lineNumber, getTokenTypeString(tok->type)
      );
      exit(0);
    }

    if (tok->type == close) depth--;
    else if (tok->type == open) depth++;

    i++;
  }

  return i - 1;  // Return index of closing token
}

//  Parse list literal [elem1, elem2, ...]
// Supports: empty [], single [1], multiple [1, 2, 3], nested [[1,2],[3,4]]
AstNode *parseListLiteral(TokenArray *arr, int start, int length) {
  Token *head = &arr->tokens[start];
  Token *tail = &arr->tokens[start + length - 1];

  // Verify list syntax: must start with [ and end with ]
  if (head->type != TOK_LBRACKET) {
    printf("Syntax Error @ Line %i: Expected '[' at start of list.\n", head->lineNumber);
    exit(0);
  }
  if (tail->type != TOK_RBRACKET) {
    printf("Syntax Error @ Line %i: Expected ']' at end of list.\n", tail->lineNumber);
    exit(0);
  }

  // Create list node
  AstNode *listNode = AstNode_new(NULL, OP_LIST, head->lineNumber);

  // Empty list: []
  if (length == 2) {
    return listNode;
  }

  // Parse elements between [ and ]
  int i = start + 1;  // Skip [
  int elemStart = i;

  while (i < start + length - 1) {  // Stop before ]
    Token *tok = &arr->tokens[i];

    // Skip nested structures to find element boundaries
    if (tok->type == TOK_APPLYOPEN) {
      i = skipClosure(arr, i, TOK_APPLYOPEN, TOK_APPLYCLOSE);
    } else if (tok->type == TOK_FUNCOPEN) {
      i = skipClosure(arr, i, TOK_FUNCOPEN, TOK_FUNCCLOSE);
    } else if (tok->type == TOK_LBRACKET) {
      // Nested list - skip to find its end
      i = skipClosure(arr, i, TOK_LBRACKET, TOK_RBRACKET);
    }

    // Check if we're at comma or end of list
    Token *nextTok = (i + 1 < start + length) ? &arr->tokens[i + 1] : NULL;
    if (nextTok && (nextTok->type == TOK_COMMA || i + 1 == start + length - 1)) {
      // Parse element from elemStart to i (inclusive)
      int elemLength = i - elemStart + 1;
      if (elemLength > 0) {
        AstNode *element = parseValue(arr, elemStart, elemLength);
        AstNode_addChild(listNode, element);
      }

      if (nextTok->type == TOK_COMMA) {
        i += 2;  // Skip comma
        elemStart = i;
        continue;
      } else {
        // End of list
        break;
      }
    }

    i++;
  }

  return listNode;
}

//  Parse application using array indexing
AstNode *parseApplication(TokenArray *arr, int start, int length) {
  Token *head = &arr->tokens[start];

  if (head->type != TOK_APPLYOPEN) {
    printf(
      "Syntax Error @ Line %i: Unexpected %s token.\n",
      head->lineNumber, getTokenTypeString(head->type)
    );
    exit(0);
  }

  AstNode *res = AstNode_new(NULL, OP_APPLICATION, head->lineNumber);
  AstNode *p_lastChild = NULL;

  int i = start + 1;
  int end = start + length;

  while (i < end && arr->tokens[i].type != TOK_APPLYCLOSE) {
    Token *curr = &arr->tokens[i];

    if (curr->type == TOK_APPLYOPEN || curr->type == TOK_FUNCOPEN) {
      // Case of value which is closure
      int closureStart = i;
      int closureEnd = skipClosure(arr, i, curr->type, curr->type + 1);
      int closureLength = closureEnd - closureStart + 1;

      AstNode_appendChild(res, &p_lastChild, parseValue(arr, closureStart, closureLength));
      i = closureEnd + 1;

    } else if (curr->type == TOK_LBRACKET) {
      //  Case of list literal [elem1, elem2, ...]
      int listStart = i;
      int listEnd = skipClosure(arr, i, TOK_LBRACKET, TOK_RBRACKET);
      int listLength = listEnd - listStart + 1;

      AstNode_appendChild(res, &p_lastChild, parseValue(arr, listStart, listLength));
      i = listEnd + 1;

    } else if (curr->type == TOK_IDENTIFIER &&
               i + 2 < end &&
               arr->tokens[i + 1].type == TOK_DOT &&
               arr->tokens[i + 2].type == TOK_IDENTIFIER) {
      // Case of qualified name: ns.identifier
      AstNode_appendChild(res, &p_lastChild, parseValue(arr, i, 3));
      i += 3;

    } else {
      // Case of single token value
      AstNode_appendChild(res, &p_lastChild, parseValue(arr, i, 1));
      i++;
    }
  }

  if (i >= end || arr->tokens[i].type != TOK_APPLYCLOSE) {
    printf(
      "Syntax Error @ Line %i: Application not closed.\n",
      arr->tokens[i < arr->count ? i : arr->count - 1].lineNumber
    );
    exit(0);
  }

  return res;
}

//  Parse value using array indexing
AstNode *parseValue(TokenArray *arr, int start, int length) {
  Token *head = &arr->tokens[start];

  if (length == 1) {
    // Single token: int, float, string, or identifier
    if (head->type == TOK_STRING) {
      return AstNode_new(head->val, OP_STRING, head->lineNumber);
    } else if (head->type == TOK_FLOAT) {
      return AstNode_new(head->val, OP_FLOAT, head->lineNumber);
    } else if (head->type == TOK_INT) {
      return AstNode_new(head->val, OP_INT, head->lineNumber);
    } else if (head->type == TOK_IDENTIFIER) {
      return AstNode_new(head->val, OP_IDENTIFIER, head->lineNumber);
    } else {
      printf(
        "Syntax Error @ Line %i: Unexpected %s token.\n",
        head->lineNumber, getTokenTypeString(head->type)
      );
      exit(0);
    }

  } else if (length == 3 &&
             head->type == TOK_IDENTIFIER &&
             arr->tokens[start + 1].type == TOK_DOT &&
             arr->tokens[start + 2].type == TOK_IDENTIFIER) {
    // Qualified name: ns.identifier
    int namespace_len = strlen(head->val);
    int member_len = strlen(arr->tokens[start + 2].val);
    char *qualified_name = malloc(namespace_len + 1 + member_len + 1);
    strcpy(qualified_name, head->val);
    qualified_name[namespace_len] = '.';
    strcpy(qualified_name + namespace_len + 1, arr->tokens[start + 2].val);

    return AstNode_new(qualified_name, OP_QUALIFIED, head->lineNumber);

  } else if (length > 1) {
    // Application, function, or list literal
    if (head->type == TOK_APPLYOPEN) {
      return parseApplication(arr, start, length);
    } else if (head->type == TOK_FUNCOPEN) {
      return parseFunction(arr, start, length);
    } else if (head->type == TOK_LBRACKET) {
      //  List literal [elem1, elem2, ...]
      return parseListLiteral(arr, start, length);
    } else {
      printf(
        "Syntax Error @ Line %i: Unexpected %s token.\n",
        head->lineNumber, getTokenTypeString(head->type)
      );
      exit(0);
    }
  }

  return NULL;
}

//  Parse assignment using array indexing
//  Support 'mut' keyword for mutable variables
// Syntax: x = 5 (immutable) or mut x = 5 (mutable)
AstNode *parseAssignment(TokenArray *arr, int start, int length) {
  Token *head = &arr->tokens[start];
  int isMutable = 0;
  int offset = 0;

  // Check if this starts with 'mut' keyword
  if (head->type == TOK_MUT) {
    isMutable = 1;
    offset = 1;

    // Move to the identifier after 'mut'
    if (start + offset >= start + length) {
      printf(
        "Syntax Error @ Line %i: Expected identifier after 'mut'.\n",
        head->lineNumber
      );
      exit(0);
    }

    head = &arr->tokens[start + offset];
  }

  if (length < 3 + offset) {
    printf(
      "Syntax Error @ Line %i: Incomplete assignment.\n",
      head->lineNumber
    );
    exit(0);
  }

  if (head->type != TOK_IDENTIFIER) {
    printf(
      "Syntax Error @ Line %i: Unexpected %s token.\n",
      head->lineNumber, getTokenTypeString(head->type)
    );
    exit(0);
  }

  if (arr->tokens[start + offset + 1].type != TOK_ASSIGNMENT) {
    printf(
      "Syntax Error @ Line %i: Unexpected %s token.\n",
      arr->tokens[start + offset + 1].lineNumber, getTokenTypeString(arr->tokens[start + offset + 1].type)
    );
    exit(0);
  }

  // Create assignment node
  AstNode *res = AstNode_new(NULL, OP_ASSIGNMENT, head->lineNumber);
  res->isMutable = isMutable;  // Mark if declared with mut
  AstNode_addChild(res, AstNode_new(head->val, OP_IDENTIFIER, head->lineNumber));
  AstNode_addChild(res, parseValue(arr, start + offset + 2, length - offset - 2));

  return res;
}

//  Parse return using array indexing
AstNode *parseReturn(TokenArray *arr, int start, int length) {
  Token *head = &arr->tokens[start];

  if (length < 2) {
    printf(
      "Syntax Error @ Line %i: Incomplete return.\n",
      head->lineNumber
    );
    exit(0);
  }

  if (head->type != TOK_RETURN) {
    printf(
      "Syntax Error @ Line %i: Unexpected %s token.\n",
      head->lineNumber, getTokenTypeString(head->type)
    );
    exit(0);
  }

  // Create return node
  AstNode *res = AstNode_new(NULL, OP_RETURN, head->lineNumber);
  AstNode_addChild(res, parseValue(arr, start + 1, length - 1));

  return res;
}

//  Parse function using array indexing
AstNode *parseFunction(TokenArray *arr, int start, int length) {
  Token *head = &arr->tokens[start];

  if (head->type != TOK_FUNCOPEN) {
    printf(
      "Syntax Error @ Line %i: Unexpected %s token.\n",
      head->lineNumber, getTokenTypeString(head->type)
    );
    exit(0);
  }

  AstNode *res = AstNode_new(NULL, OP_FUNCTION, head->lineNumber);
  AstNode *p_lastChild = NULL;

  int i = start + 1;
  int end = start + length;
  int arrowIndex = -1;

  // Find arrow token
  int depth = 0;
  for (int j = start + 1; j < end; j++) {
    Token *tok = &arr->tokens[j];

    if (tok->type == TOK_FUNCOPEN || tok->type == TOK_APPLYOPEN) depth++;
    else if (tok->type == TOK_FUNCCLOSE || tok->type == TOK_APPLYCLOSE) depth--;
    else if (tok->type == TOK_ARROW && depth == 0) {
      arrowIndex = j;
      break;
    }
  }

  // Parse parameters (before arrow) or no-param function
  if (arrowIndex == -1) {
    // No arrow, entire body is code
    // Parse as statement list
    i = start + 1;

    while (i < end && arr->tokens[i].type != TOK_FUNCCLOSE) {
      Token *curr = &arr->tokens[i];
      int stmtStart = i;
      int stmtEnd = i;

      // Find statement end
      if (curr->type == TOK_APPLYOPEN || curr->type == TOK_FUNCOPEN) {
        stmtEnd = skipClosure(arr, i, curr->type, curr->type + 1);
      } else if (curr->type == TOK_RETURN) {
        // Find return value end
        if (i + 1 < end) {
          Token *next = &arr->tokens[i + 1];
          if (next->type == TOK_APPLYOPEN || next->type == TOK_FUNCOPEN) {
            stmtEnd = skipClosure(arr, i + 1, next->type, next->type + 1);
          } else {
            stmtEnd = i + 1;
          }
        }
      } else {
        // Find assignment or expression
        depth = 0;
        for (int j = i; j < end; j++) {
          Token *tok = &arr->tokens[j];

          if (tok->type == TOK_FUNCOPEN || tok->type == TOK_APPLYOPEN) depth++;
          else if (tok->type == TOK_FUNCCLOSE || tok->type == TOK_APPLYCLOSE) {
            if (depth == 0) break;
            depth--;
          } else if (tok->type == TOK_ASSIGNMENT && depth == 0) {
            // Assignment: find value after =
            for (int k = j + 1; k < end; k++) {
              Token *valTok = &arr->tokens[k];
              if (valTok->type == TOK_APPLYOPEN || valTok->type == TOK_FUNCOPEN) {
                stmtEnd = skipClosure(arr, k, valTok->type, valTok->type + 1);
                break;
              } else if (k + 1 < end &&
                         (arr->tokens[k + 1].type == TOK_FUNCCLOSE ||
                          arr->tokens[k + 1].type == TOK_RETURN)) {
                stmtEnd = k;
                break;
              }
            }
            break;
          }
        }
      }

      int stmtLength = stmtEnd - stmtStart + 1;
      AstNode_appendChild(res, &p_lastChild, parseStatement(arr, stmtStart, stmtLength));
      i = stmtEnd + 1;
    }

  } else {
    // Has arrow: parse parameters then body
    // Parameters
    i = start + 1;
    while (i < arrowIndex) {
      Token *curr = &arr->tokens[i];
      if (curr->type == TOK_IDENTIFIER) {
        AstNode_appendChild(res, &p_lastChild, AstNode_new(curr->val, OP_IDENTIFIER, curr->lineNumber));
      }
      i++;
    }

    // Body (after arrow)
    i = arrowIndex + 1;
    while (i < end && arr->tokens[i].type != TOK_FUNCCLOSE) {
      Token *curr = &arr->tokens[i];
      int stmtStart = i;
      int stmtEnd = i;

      // Find statement end (same logic as above)
      if (curr->type == TOK_APPLYOPEN || curr->type == TOK_FUNCOPEN) {
        stmtEnd = skipClosure(arr, i, curr->type, curr->type + 1);
      } else if (curr->type == TOK_RETURN) {
        if (i + 1 < end) {
          Token *next = &arr->tokens[i + 1];
          if (next->type == TOK_APPLYOPEN || next->type == TOK_FUNCOPEN) {
            stmtEnd = skipClosure(arr, i + 1, next->type, next->type + 1);
          } else {
            stmtEnd = i + 1;
          }
        }
      } else {
        // Assignment or expression
        depth = 0;
        for (int j = i; j < end; j++) {
          Token *tok = &arr->tokens[j];
          if (tok->type == TOK_FUNCOPEN || tok->type == TOK_APPLYOPEN) depth++;
          else if (tok->type == TOK_FUNCCLOSE || tok->type == TOK_APPLYCLOSE) {
            if (depth == 0) break;
            depth--;
          } else if (tok->type == TOK_ASSIGNMENT && depth == 0) {
            for (int k = j + 1; k < end; k++) {
              Token *valTok = &arr->tokens[k];
              if (valTok->type == TOK_APPLYOPEN || valTok->type == TOK_FUNCOPEN) {
                stmtEnd = skipClosure(arr, k, valTok->type, valTok->type + 1);
                break;
              } else if (k + 1 < end &&
                         (arr->tokens[k + 1].type == TOK_FUNCCLOSE ||
                          arr->tokens[k + 1].type == TOK_RETURN)) {
                stmtEnd = k;
                break;
              }
            }
            break;
          }
        }
      }

      int stmtLength = stmtEnd - stmtStart + 1;
      AstNode_appendChild(res, &p_lastChild, parseStatement(arr, stmtStart, stmtLength));
      i = stmtEnd + 1;
    }
  }

  if (i >= arr->count || arr->tokens[i].type != TOK_FUNCCLOSE) {
    printf(
      "Syntax Error @ Line %i: Function not closed.\n",
      arr->tokens[i < arr->count ? i : arr->count - 1].lineNumber
    );
    exit(0);
  }

  return res;
}

//  Parse statement using array indexing
AstNode *parseStatement(TokenArray *arr, int start, int length) {
  Token *head = &arr->tokens[start];

  if (length < 1) {
    printf("Syntax Error @ Line %i: Empty statement.\n", head->lineNumber);
    exit(0);
  }

  // Create statement node
  AstNode *res = AstNode_new(NULL, OP_STATEMENT, head->lineNumber);
  AstNode *p_lastChild = NULL;

  int i = start;
  int end = start + length;

  while (i < end) {
    Token *curr = &arr->tokens[i];

    if (curr->type == TOK_RETURN) {
      // Return statement
      int returnStart = i;
      int returnEnd = i;

      if (i + 1 < end) {
        Token *next = &arr->tokens[i + 1];
        if (next->type == TOK_APPLYOPEN || next->type == TOK_FUNCOPEN) {
          returnEnd = skipClosure(arr, i + 1, next->type, next->type + 1);
        } else {
          returnEnd = i + 1;
        }
      }

      int returnLength = returnEnd - returnStart + 1;
      AstNode_appendChild(res, &p_lastChild, parseReturn(arr, returnStart, returnLength));
      i = returnEnd + 1;

    } else if (curr->type == TOK_MUT && i + 1 < end && arr->tokens[i + 1].type == TOK_IDENTIFIER && i + 2 < end && arr->tokens[i + 2].type == TOK_ASSIGNMENT) {
      // Mutable assignment statement (mut x = value)
      int assignStart = i;
      int assignEnd = i + 2;  // Start from =

      if (i + 3 < end) {
        Token *valTok = &arr->tokens[i + 3];
        if (valTok->type == TOK_APPLYOPEN || valTok->type == TOK_FUNCOPEN) {
          assignEnd = skipClosure(arr, i + 3, valTok->type, valTok->type + 1);
        } else if (valTok->type == TOK_LBRACKET) {
          //  List literal assignment
          assignEnd = skipClosure(arr, i + 3, TOK_LBRACKET, TOK_RBRACKET);
        } else {
          assignEnd = i + 3;
        }
      }

      int assignLength = assignEnd - assignStart + 1;
      AstNode_appendChild(res, &p_lastChild, parseAssignment(arr, assignStart, assignLength));
      i = assignEnd + 1;

    } else if (curr->type == TOK_IDENTIFIER && i + 1 < end && arr->tokens[i + 1].type == TOK_ASSIGNMENT) {
      // Assignment statement
      int assignStart = i;
      int assignEnd = i + 1;  // Start from =

      if (i + 2 < end) {
        Token *valTok = &arr->tokens[i + 2];
        if (valTok->type == TOK_APPLYOPEN || valTok->type == TOK_FUNCOPEN) {
          assignEnd = skipClosure(arr, i + 2, valTok->type, valTok->type + 1);
        } else if (valTok->type == TOK_LBRACKET) {
          //  List literal assignment
          assignEnd = skipClosure(arr, i + 2, TOK_LBRACKET, TOK_RBRACKET);
        } else {
          assignEnd = i + 2;
        }
      }

      int assignLength = assignEnd - assignStart + 1;
      AstNode_appendChild(res, &p_lastChild, parseAssignment(arr, assignStart, assignLength));
      i = assignEnd + 1;

    } else if (curr->type == TOK_APPLYOPEN || curr->type == TOK_FUNCOPEN) {
      // Expression (application or function)
      int exprStart = i;
      int exprEnd = skipClosure(arr, i, curr->type, curr->type + 1);
      int exprLength = exprEnd - exprStart + 1;

      AstNode_appendChild(res, &p_lastChild, parseValue(arr, exprStart, exprLength));
      i = exprEnd + 1;

    } else {
      // Single token expression (identifier, int, float, string)
      AstNode_appendChild(res, &p_lastChild, parseValue(arr, i, 1));
      i++;
    }
  }

  return res;
}

//  Parse program using array
AstNode *parseProgram(TokenArray *arr) {
  if (arr == NULL || arr->count == 0) {
    printf("Error: Empty token array\n");
    exit(0);
  }

  Token *head = &arr->tokens[0];
  Token *end = &arr->tokens[arr->count - 1];

  if (head->type != TOK_START) {
    printf("Syntax Error @ Line 1: Missing start token.\n");
    exit(0);
  }

  if (end->type != TOK_END) {
    printf("Syntax Error @ Line %i: Missing end token.\n", end->lineNumber);
    exit(0);
  }

  // Parse all tokens between START and END as a single statement
  // (Franz programs are one big statement that may contain multiple sub-statements)
  return parseStatement(arr, 1, arr->count - 2);
}
