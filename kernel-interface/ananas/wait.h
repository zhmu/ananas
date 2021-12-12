/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_WAIT_H__
#define __ANANAS_WAIT_H__

#define WNOHANG 1
#define WUNTRACED 2

#define W_EXTRACT_STATUS(n) ((n) >> 16)
#define W_STATUS_EXITED     1
#define W_STATUS_SIGNALLED  2
#define W_STATUS_STOPPED    3

#define W_EXTRACT_VALUE(n)  ((n) & 0xffff)
#define W_VALUE_COREDUMP    0x100

#define W_MAKE(s,v)         (((s) << 16) | (v))

#define WIFEXITED(n)        (W_EXTRACT_STATUS(n) == W_STATUS_EXITED)
#define WIFSIGNALED(n)      (W_EXTRACT_STATUS(n) == W_STATUS_SIGNALLED)
#define WIFSTOPPED(n)       (W_EXTRACT_STATUS(n) == W_STATUS_STOPPED)

#define WEXITSTATUS(n)      (W_EXTRACT_VALUE(n) & 0xff)
#define WTERMSIG(n)         (W_EXTRACT_VALUE(n) & 0xff)
#define WSTOPSIG(n)         (W_EXTRACT_VALUE(n) & 0xff)

#define WCOREDUMP(n)        ((W_EXTRACT_VALUE(n) & W_VALUE_COREDUMP) != 0)

#endif /* __ANANAS_WAIT_H__ */
