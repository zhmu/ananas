/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <stdlib.h>
#include <string.h>

extern char** environ;

static size_t get_environment_size()
{
    size_t n = 0;
    for (char** e = environ; *e != NULL; e++, n++)
        /* nothing */;
    return n;
}

int putenv(char* s)
{
    // s is in form 'FOO=bar', 'FOO=' or 'FOO'; if it's not the former, it
    // means we have to remove 's' from the environment
    const char* p = strchr(s, '=');
    size_t envname_len = s - p;
    if (p == NULL)
        envname_len = strlen(s);
    if (s[envname_len] == '\0')
        return unsetenv(s); // we handle 'FOO=' here as an extension

    // See if we can overwrite something - we don't bother to free old strings
    // as it's not really worth the hassle (
    for (char** e = environ; *e != NULL; e++) {
        // Match s up to and including the '='
        if (strncmp(*e, s, envname_len + 1) != 0)
            continue;
        *e = s;
        return 0;
    }

    // Not yet present, must add it. This means we either duplicate environ[]
    // or we reallocate it if we have already done so before
    size_t env_size = get_environment_size();
    static char** initial_environment = NULL;
    if (initial_environment == NULL)
        initial_environment = environ;
    char** next_env;
    if (environ == initial_environment) {
        // Not yet relocated; duplicate environ as we are going to change it
        next_env = malloc((env_size + 2) * sizeof(char*));
        if (next_env == NULL)
            return -1;
        memcpy(next_env, environ, env_size * sizeof(char*));
    } else {
        // Just re-size the environment
        next_env = realloc(environ, sizeof(char*) * (env_size + 2));
        if (next_env == NULL)
            return -1;
    }
    next_env[env_size++] = s;
    next_env[env_size++] = NULL;
    environ = next_env;
    return 0;
}
