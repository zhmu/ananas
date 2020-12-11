/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#define MD_VMSPACE_FIELDS uint64_t* vs_md_pagedir;

struct VMSpace;

extern VMSpace kernel_vmspace;
