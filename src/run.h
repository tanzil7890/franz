#ifndef RUN_H
#define RUN_H
#include <stdbool.h>

//  Run function now uses bytecode VM by default
// prototypes
int run(char *code, long length, int argc, char *argv[], bool debug, bool enable_tco);

#endif
