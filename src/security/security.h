#ifndef SECURITY_H
#define SECURITY_H

#include "../scope.h"
#include "../generic.h"

/**
 * Capability-Based Security System for Franz
 *
 * Provides sandboxed execution environments where imported code
 * only has access to explicitly granted capabilities.
 *
 * This prevents RCE (Remote Code Execution) vulnerabilities by:
 * - Creating isolated scopes with NO parent access
 * - Only granting explicitly requested capabilities
 * - Blocking dangerous operations (shell, file I/O) by default
 */

/**
 * Grant a single capability to a scope
 *
 * @param p_scope - The isolated capability scope to grant to
 * @param capability - The capability string (e.g., "print", "files:read", "shell")
 * @param lineNumber - Line number for error reporting
 * @return 0 on success, -1 on unknown capability
 */
int Security_grantCapability(Scope *p_scope, const char *capability, int lineNumber);

/**
 * Create a new isolated capability scope
 *
 * Creates a scope with NO parent, preventing access to caller's
 * scope and global variables.
 *
 * @return New isolated Scope with no parent
 */
Scope *Security_createIsolatedScope(void);

/**
 * Seed a scope with a list of capabilities
 *
 * @param p_scope - The isolated scope to seed
 * @param capabilities - List Generic containing capability strings
 * @param lineNumber - Line number for error reporting
 * @return 0 on success, -1 on error
 */
int Security_seedCapabilities(Scope *p_scope, Generic *capabilities, int lineNumber);

#endif
