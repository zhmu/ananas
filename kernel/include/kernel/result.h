/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_RESULT_H__
#define __ANANAS_RESULT_H__

#include <ananas/statuscode.h>

class Result
{
  public:
    explicit constexpr Result(const statuscode_t statuscode) : r_StatusCode(statuscode) { }

    bool IsSuccess() const { return ananas_statuscode_is_success(r_StatusCode); }

    bool IsFailure() const { return !IsSuccess(); }

    unsigned int AsErrno() const { return ananas_statuscode_extract_errno(r_StatusCode); }

    constexpr statuscode_t AsStatusCode() const { return r_StatusCode; }
    constexpr auto AsValue() const { return static_cast<unsigned int>(r_StatusCode); }

    static Result Failure(unsigned int errno) { return Result(ananas_statuscode_make_failure(errno)); }

    static Result Success(unsigned int value = 0) { return Result(ananas_statuscode_make_success(value)); }

  private:
    statuscode_t r_StatusCode;
};

#define RESULT_MAKE_FAILURE(errno) Result::Failure(errno)

#endif // __ANANAS_RESULT_H__
