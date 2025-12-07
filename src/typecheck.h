#ifndef TYPECHECK_H
#define TYPECHECK_H

#include "typeinfer.h"
#include "types.h"

// Add standard library type signatures to environment
void typecheck_add_stdlib(TypeEnv *env);

#endif
