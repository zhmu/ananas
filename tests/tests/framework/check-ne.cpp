/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
// SUMMARY:Trivial EXPECT_NE() test

#include "framework.h"

TEST_BODY_BEGIN { EXPECT_NE(0, 1); }
TEST_BODY_END
