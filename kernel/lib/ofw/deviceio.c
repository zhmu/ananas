/* Implements commands as described in 6.3.2.3: device i/o */
#include <sys/lib.h> 
#include <ofw.h> 

ofw_cell_t
ofw_open(const char* device)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	device_specifier;
		ofw_cell_t	ihandle;
	} args = {
		.service = (ofw_cell_t)"open",
		.n_args    = 1,
		.n_returns = 1,
	};

	args.device_specifier = (ofw_cell_t)device;
	ofw_call(&args);
	return args.ihandle;
}

void
ofw_close(ofw_cell_t ihandle)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	ihandle;
	} args = {
		.service = (ofw_cell_t)"close",
		.n_args    = 1,
		.n_returns = 0,
	};

	args.ihandle = ihandle;
	ofw_call(&args);
}

int
ofw_read(ofw_cell_t ihandle, void* buf, unsigned int length)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	ihandle;
		ofw_cell_t	addr;
		ofw_cell_t	len;
		ofw_cell_t	actual;
	} args = {
		.service = (ofw_cell_t)"read",
		.n_args    = 3,
		.n_returns = 1,
	};

	args.ihandle = ihandle;
	args.addr = (ofw_cell_t)buf;
	args.len = length;
	ofw_call(&args);
	return args.actual;
}

int
ofw_write(ofw_cell_t ihandle, const void* buf, unsigned int length)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	ihandle;
		ofw_cell_t	addr;
		ofw_cell_t	len;
		ofw_cell_t	actual;
	} args = {
		.service = (ofw_cell_t)"write",
		.n_args    = 3,
		.n_returns = 1,
	};

	args.ihandle = ihandle;
	args.addr = (ofw_cell_t)buf;
	args.len = length;
	ofw_call(&args);
	return args.actual;
}

int
ofw_seek(ofw_cell_t ihandle, uint64_t position)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	ihandle;
		ofw_cell_t	pos_hi;
		ofw_cell_t	pos_lo;
		ofw_cell_t	status;
	} args = {
		.service = (ofw_cell_t)"seek",
		.n_args    = 3,
		.n_returns = 1,
	};

	args.ihandle = ihandle;
	args.pos_hi = (position >> 32) & 0xffffffff;
	args.pos_lo = (position      ) & 0xffffffff;
	ofw_call(&args);
	return args.status;
}

/* vim:set ts=2 sw=2: */
