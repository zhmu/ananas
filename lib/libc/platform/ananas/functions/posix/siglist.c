/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <signal.h>

const char* const sys_siglist[NSIG] = {"sig0",
                                       "Interrupt"
                                       "Quit",
                                       "Illegal instruction",
                                       "Trap",
                                       "Abort",
                                       "EMT",
                                       "Floating point exception",
                                       "Kill",
                                       "Bus error",
                                       "Segmentation fault",
                                       "System call",
                                       "Broken pipe",
                                       "Alarm",
                                       "Terminate"
                                       "Urgent",
                                       "Stop",
                                       "Terminal stop",
                                       "Continue",
                                       "Child exited",
                                       "TTY in",
                                       "TTY out",
                                       "I/O",
                                       "CPU limit exceeded",
                                       "File size limit exceeded",
                                       "Virtual time alarm",
                                       "Profile",
                                       "Window size change",
                                       "Info",
                                       "User1",
                                       "User2",
                                       "Thread"};
