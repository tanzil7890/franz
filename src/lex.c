#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lex.h"
#include "tokens.h"
#include "string.h"

// handles errors while scanning chars in a string
void handleStringError(char c, int lineNumber) {
  if (c == '\n') {
    printf("Syntax Error @ Line %i: Unexpected new line before string closed.\n", lineNumber);
    exit(0);
  }

  if (c == '\0') {
    printf("Syntax Error @ Line %i: Unexpected end of file before string closed.\n", lineNumber);
    exit(0);
  }
}

//  Lex code into array-based token list
TokenArray* lex(char *code, int fileLength) {
  // Create new token array
  TokenArray *tokens = TokenArray_new();

  // Add START token
  TokenArray_push(tokens, NULL, TOK_START, 1);

  // line number
  int lineNumber = 1;

  // for each char (including terminator, helps us not need to push number tokens if they are last)
  int i = 0;
  while (i < fileLength) {
    char c = code[i];

    if (c == '\n') lineNumber++;

    if (c == '/' && code[i + 1] == '/') {
      // comments
      while (code[i] != '\n' && i < fileLength) i++;
      lineNumber++;
    } else if (c == '(') {
      TokenArray_push(tokens, NULL, TOK_APPLYOPEN, lineNumber);
    } else if (c == ')') {
      TokenArray_push(tokens, NULL, TOK_APPLYCLOSE, lineNumber);
    } else if (c == '[') {
      // : List literal syntax
      TokenArray_push(tokens, NULL, TOK_LBRACKET, lineNumber);
    } else if (c == ']') {
      // : List literal syntax
      TokenArray_push(tokens, NULL, TOK_RBRACKET, lineNumber);
    } else if (c == ',') {
      // : Comma separator for list elements
      TokenArray_push(tokens, NULL, TOK_COMMA, lineNumber);
    } else if (c == '=') {
      TokenArray_push(tokens, NULL, TOK_ASSIGNMENT, lineNumber);
    } else if (c == '{') {
      TokenArray_push(tokens, NULL, TOK_FUNCOPEN, lineNumber);
    } else if (c == '}') {
      TokenArray_push(tokens, NULL, TOK_FUNCCLOSE, lineNumber);
    } else if (c == '-' && code[i + 1] == '>') {
      TokenArray_push(tokens, NULL, TOK_ARROW, lineNumber);
      i++;
    } else if (c == '<' && code[i + 1] == '-') {
      TokenArray_push(tokens, NULL, TOK_RETURN, lineNumber);
      i++;
    } else if (c == '"') {
      
      // record first char in string
      int stringStart = i + 1;

      // go to first char after quotes
      i++;

      // count to last char in string (last quote)
      while (code[i] != '"') {

        // error handling
        handleStringError(code[i], lineNumber);
        
        // skip escape codes
        if (code[i] == '\\') {
          i++;
          handleStringError(code[i], lineNumber);
        }

        i++;
      }

      // get substring and add token
      char *val = malloc(i - stringStart + 1);
      strncpy(val, &code[stringStart], i - stringStart);
      val[i - stringStart] = '\0';

      TokenArray_push(tokens, parseString(val), TOK_STRING, lineNumber);

      free(val);

    } else if (c == '0' && (code[i + 1] == 'x' || code[i + 1] == 'X')) {
      //  Hexadecimal integer or float literal (0x1A or 0x1.5p2)
      int numStart = i;
      i += 2;  // Skip '0x'

      bool isHexFloat = false;
      bool hasDigits = false;

      // Parse hex digits before potential decimal point
      while (isxdigit((unsigned char) code[i])) {
        hasDigits = true;
        i++;
      }

      // Check for hexadecimal float (0x1.5p2)
      if (code[i] == '.' && isxdigit((unsigned char) code[i + 1])) {
        isHexFloat = true;
        i++;  // Skip '.'
        while (isxdigit((unsigned char) code[i])) {
          i++;
        }
      }

      // Check for binary exponent (p or P) - required for hex floats
      if ((code[i] == 'p' || code[i] == 'P')) {
        isHexFloat = true;
        i++;  // Skip 'p'

        // Optional sign
        if (code[i] == '+' || code[i] == '-') {
          i++;
        }

        // Exponent must have digits
        if (!isdigit((unsigned char) code[i])) {
          printf("Syntax Error @ Line %i: Hexadecimal float requires exponent after 'p'.\n", lineNumber);
          exit(0);
        }

        while (isdigit((unsigned char) code[i])) {
          i++;
        }
      }

      if (!hasDigits && !isHexFloat) {
        printf("Syntax Error @ Line %i: Invalid hexadecimal literal - no digits after '0x'.\n", lineNumber);
        exit(0);
      }

      // Extract the hex literal
      char *val = malloc(i - numStart + 1);
      strncpy(val, &code[numStart], i - numStart);
      val[i - numStart] = '\0';

      if (isHexFloat) {
        TokenArray_push(tokens, val, TOK_FLOAT, lineNumber);
      } else {
        TokenArray_push(tokens, val, TOK_INT, lineNumber);
      }

      i--;

    } else if (c == '0' && (code[i + 1] == 'b' || code[i + 1] == 'B')) {
      //  Binary integer literal (0b1010)
      int numStart = i;
      i += 2;  // Skip '0b'

      bool hasDigits = false;
      while (code[i] == '0' || code[i] == '1') {
        hasDigits = true;
        i++;
      }

      if (!hasDigits) {
        printf("Syntax Error @ Line %i: Invalid binary literal - no digits after '0b'.\n", lineNumber);
        exit(0);
      }

      char *val = malloc(i - numStart + 1);
      strncpy(val, &code[numStart], i - numStart);
      val[i - numStart] = '\0';

      TokenArray_push(tokens, val, TOK_INT, lineNumber);
      i--;

    } else if (c == '0' && (code[i + 1] == 'o' || code[i + 1] == 'O')) {
      //  Octal integer literal (0o17)
      int numStart = i;
      i += 2;  // Skip '0o'

      bool hasDigits = false;
      while (code[i] >= '0' && code[i] <= '7') {
        hasDigits = true;
        i++;
      }

      if (!hasDigits) {
        printf("Syntax Error @ Line %i: Invalid octal literal - no digits after '0o'.\n", lineNumber);
        exit(0);
      }

      char *val = malloc(i - numStart + 1);
      strncpy(val, &code[numStart], i - numStart);
      val[i - numStart] = '\0';

      TokenArray_push(tokens, val, TOK_INT, lineNumber);
      i--;

    } else if (isdigit((unsigned char) c) > 0 || (c == '-' && isdigit((unsigned char) code[i + 1]) > 0)) {

      // record first char in int
      int numStart = i;

      // float flag
      bool isFloat = false;

      // increment
      i++;

      // increment until char is not a valid number char
      while (isdigit((unsigned char) code[i]) > 0 || code[i] == '.') {

        // handle float flag and protect against multiple points
        if (code[i] == '.') {
          // Check if this is a decimal point or member access
          // It's a decimal if the next char is a digit
          // It's member access if next char is NOT a digit
          if (isdigit((unsigned char) code[i + 1]) > 0) {
            // This is a decimal point in a float
            if (isFloat) {
              // case were we saw a point before
              printf("Syntax Error @ Line %i: Multiple decimal points in single number.\n", lineNumber);
              exit(0);
            } else {
              isFloat = true;
            }
          } else {
            // This dot is NOT part of the number - it's member access
            // Stop parsing the number here
            break;
          }
        }

        i++;
      }

      // : Check for scientific notation (e.g., 1.5e2, 1e-3, 2.5E+10)
      if (code[i] == 'e' || code[i] == 'E') {
        isFloat = true;  // Scientific notation always produces a float
        i++;  // Skip 'e' or 'E'

        // Optional sign after 'e'
        if (code[i] == '+' || code[i] == '-') {
          i++;
        }

        // Must have at least one digit after 'e' or 'e+'/'e-'
        if (!isdigit((unsigned char) code[i])) {
          printf("Syntax Error @ Line %i: Invalid scientific notation - expected digit after 'e'.\n", lineNumber);
          exit(0);
        }

        // Parse exponent digits
        while (isdigit((unsigned char) code[i]) > 0) {
          i++;
        }
      }

      // get substring and add token
      char *val = malloc(i - numStart + 1);
      strncpy(val, &code[numStart], i - numStart);
      val[i - numStart] = '\0';

      if (isFloat) TokenArray_push(tokens, val, TOK_FLOAT, lineNumber);
      else TokenArray_push(tokens, val, TOK_INT, lineNumber);

      // make sure to go back to last char of number
      i--;

    } else if (c == '.' && !isdigit((unsigned char) code[i + 1])) {
      // Dot that is NOT part of a float (member access operator)
      TokenArray_push(tokens, NULL, TOK_DOT, lineNumber);

    } else if (
      strchr(" \n\r\t\f\v{}()[]\"=.,", c) == NULL  // : Added [] and ,
      && !(c == '-' && code[i + 1] == '>')
      && !(c == '<' && code[i + 1] == '-')
      && !(c == '/' && code[i + 1] == '/')
    ) {
      // case of identifier
      int identifierStart = i;

      // while valid identifier char (now stops at dots, brackets, commas)
      while (
        strchr(" \n\r\t\f\v{}()[]\"=.,", code[i]) == NULL  // : Added [] and ,
        && !(code[i] == '-' && code[i + 1] == '>')
        && !(code[i] == '<' && code[i + 1] == '-')
        && !(code[i] == '/' && code[i + 1] == '/')
      ) i++;

      // get substring and add token
      char *val = malloc(i - identifierStart + 1);
      strncpy(val, &code[identifierStart], i - identifierStart);
      val[i - identifierStart] = '\0';

      // Check if this is the 'sig', 'as', or 'mut' keyword
      if (strcmp(val, "sig") == 0) {
        TokenArray_push(tokens, NULL, TOK_SIG, lineNumber);
        free(val);  // Free since we don't need the string value
      } else if (strcmp(val, "as") == 0) {
        TokenArray_push(tokens, NULL, TOK_AS, lineNumber);
        free(val);  // Free since we don't need the string value
      } else if (strcmp(val, "mut") == 0) {
        TokenArray_push(tokens, NULL, TOK_MUT, lineNumber);
        free(val);  // Free since we don't need the string value
      } else {
        TokenArray_push(tokens, val, TOK_IDENTIFIER, lineNumber);
      }
      
      // make sure to go back to last char of identifier
      i--;
    } else if (strchr(" \n\r\t\f\v", code[i]) == NULL) {
      // handle unexpected char
      printf("Syntax Error @ Line %i: Unexpected char \"%c\".\n", lineNumber, c);
      exit(0);
    }

    i++;
  }

  // Add END token
  TokenArray_push(tokens, NULL, TOK_END, lineNumber);

  return tokens;
}