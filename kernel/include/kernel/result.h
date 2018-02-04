#ifndef __ANANAS_RESULT_H__
#define __ANANAS_RESULT_H__

#include <ananas/statuscode.h>

class Result {
public:
	explicit Result(unsigned int fileid, unsigned int lineno, unsigned int errno)
	 : r_StatusCode(ananas_statuscode_make(fileid, lineno, errno)) {
	}

	bool IsSuccess() const {
		return r_StatusCode == 0;
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

	static Result Success() {
		return Result(0, 0, 0);
	}

	static Result FromErrNo(unsigned int errno) {
		return Result(0, 0, errno);
	}

private:
	statuscode_t r_StatusCode;
};

#define RESULT_MAKE_FAILURE(errno) \
	Result(TRACE_FILE_ID, __LINE__, (errno))

#define RESULT_PROPAGATE_FAILURE(r) \
	if (auto result_ = (r); result_.IsFailure()) \
		return result_

#endif // __ANANAS_RESULT_H__
