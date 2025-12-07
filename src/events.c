#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include "events.h"

// stores original terminal settings
struct termios orig_termios;

// stores terminal settings for while the program is running (echo off)
struct termios run_termios;

// track whether raw mode/mouse was enabled during this run
static int events_enabled = 0;

void disableRaw() {
  // reset terminal flags
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &run_termios);

  // disable mouse reporting only if we previously enabled it
  if (events_enabled && isatty(STDOUT_FILENO)) {
    // Disable all mouse tracking modes we might have enabled and show cursor
    printf("\e[?1000l\e[?1003l\e[?1006l\e[?25h\r");
    fflush(stdout);
    events_enabled = 0;
  }
}

void enableRaw() {

  // get standard settings
  struct termios raw = orig_termios;

  // set flags
  raw.c_iflag &= ~(IXON);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_oflag &= ~(OPOST);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  // write settings back into terminal
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  // enable mouse
  if (isatty(STDOUT_FILENO)) {
    printf("\e[?1003h\e[?1006h");
    fflush(stdout);
  }
  events_enabled = 1;
}

// reads a single char from stdin, assuming that raw is enabled
char readChar() {
  char c = '\0';
  read(STDIN_FILENO, &c, 1);

  // printf("%c\r\n", c);
  // handle exit
  if (iscntrl(c) && (c == 26 || c == 3)) exit(0);
  return c;
}

// returns the current event as a string, where the result is malloc
char *event() {
  // enable mouse events
  enableRaw();

  // string to return
  char *res = malloc(sizeof(char) * 1);
  res[0] = '\0';

  // read first char
  char c = readChar();

  if (c == '\e') {
    // escape code case
    // increase memory, and add \e
    res = realloc(res, sizeof(char) * (strlen(res) + 1 + 2));
    strncat(res, &c, 1);

    // read char again and add to result
    c = readChar();
    strncat(res, &c, 1);
    
    if (c == '[') {
      // control sequence introducer case
      // read more at https://en.wikipedia.org/wiki/ANSI_escape_code#CSIsection
      
      // all escape sequences are terminated by a char in the range of 0x40 – 0x7E (64 - 126)
      // we loop until we find a terminator, and append to res
      do {
        res = realloc(res, sizeof(char) * (strlen(res) + 1 + 1));
        c = readChar();
        strncat(res, &c, 1);
      } while (!(64 <= c && c <= 126));

    } else if (c == 'O') {
      // \e O is used under certain cases to access function keys
      // this escape code accepts the next char, thus this case is needed
      res = realloc(res, sizeof(char) * (strlen(res) + 1 + 1));
      c = readChar();
      strncat(res, &c, 1);
    }

  } else if (c != '\0') {
    // case where just a key was pressed, add memory, insert, and return
    res = realloc(res, sizeof(char) * (strlen(res) + 1 + 1));
    strncat(res, &c, 1);
  }
  
  // disable mouse events
  disableRaw();
  return res;
}

void exitEvents() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  if (isatty(STDOUT_FILENO)) {
    // Always ensure the cursor returns to column 0 on a TTY
    if (events_enabled) {
      // Ensure mouse modes are off and cursor is shown; return to column 0
      printf("\e[?1000l\e[?1003l\e[?1006l\e[?25h\r");
      events_enabled = 0;
    } else {
      // No special modes enabled; just realign cursor to avoid prompt indentation
      printf("\r");
    }
    fflush(stdout);
  }
}

void initEvents() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  run_termios = orig_termios;
  run_termios.c_lflag &= ~(ECHO);
  atexit(exitEvents);
}
