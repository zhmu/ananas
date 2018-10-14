#ifndef __ANANAS_RESULT_H__
#define __ANANAS_RESULT_H__

#include <ananas/statuscode.h>

class Result {
public:
	explicit Result(unsigned int fileid, unsigned int lineno, unsigned int errno)
	 : r_StatusCode(ananas_statuscode_make_failure(fileid, lineno, errno)) {
	}

	explicit Result(unsigned int value)
	 : r_StatusCode(ananas_statuscode_make_success(value))
	{
	}

	explicit Result(unsigned int value, bool)
	 : r_StatusCode(value)
	{
		// This version is used to copy a status code; used only by signals to
		// avoid messing up the interrupted result
	}

	bool IsSuccess() const {
		return ananas_statuscode_is_success(r_StatusCode);
	}

	bool IsFailure() const {
		return !IsSuccess();
	}

	unsigned int AsErrno() const {
		return ananas_statuscode_extract_errno(r_StatusCode);
	}

	statuscode_t AsStatusCode() const {
		return r_StatusCode;
	}

	statuscode_t AsValue() const {
		return r_StatusCode;
	}

	static Result Success(unsigned int value = 0) {
		return Result(value);
	}

	static Result FromErrNo(unsigned int errno) {
		return Result(0, 0, errno);
	}

private:
	statuscode_t r_StatusCode;
};

#define RESULT_MAKE_FAILURE(errno) \
	Result(TRACE_FILE_ID, __LINE__, (errno))

#endif // __ANANAS_RESULT_H__
