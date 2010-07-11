/* Implements commands as described in 6.3.2.2: device tree */
#include <sys/lib.h> 
#include <ofw.h> 

ofw_cell_t
ofw_peer(ofw_cell_t phandle)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	phandle;
		ofw_cell_t	sibling_phandle;
	} args = {
		.service = (ofw_cell_t)"peer",
		.n_args    = 1,
		.n_returns = 1,
	};

	args.phandle = phandle;
	ofw_call(&args);
	return args.sibling_phandle;
}

ofw_cell_t
ofw_child(ofw_cell_t phandle)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	phandle;
		ofw_cell_t	child_phandle;
	} args = {
		.service = (ofw_cell_t)"child",
		.n_args    = 1,
		.n_returns = 1,
	};

	args.phandle = phandle;
	ofw_call(&args);
	return args.child_phandle;
}

ofw_cell_t
ofw_finddevice(const char* device)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	devicespecifier;
		ofw_cell_t	phandle;
	} args = {
		.service = (ofw_cell_t)"finddevice",
		.n_args    = 1,
		.n_returns = 1,
	};

	args.devicespecifier = (ofw_cell_t)device;
	ofw_call(&args);
	return args.phandle;
}
int
ofw_getproplen(ofw_cell_t phandle, const char* name)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	phandle;
		ofw_cell_t	name;
		ofw_cell_t	proplen;
	} args = {
		.service = (ofw_cell_t)"getproplen",
		.n_args    = 2,
		.n_returns = 1,
	};

	args.phandle = phandle;
	args.name = (ofw_cell_t)name;
	if (ofw_call(&args) < 0)
		return -1;
	return args.proplen;
}

int
ofw_getprop(ofw_cell_t phandle, const char* name, void* buf, int length)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	phandle;
		ofw_cell_t	name;
		ofw_cell_t	buf;
		ofw_cell_t	buflen;
		ofw_cell_t	size;
	} args = {
		.service = (ofw_cell_t)"getprop",
		.n_args    = 4,
		.n_returns = 1,
	};

	args.phandle = phandle;
	args.name = (ofw_cell_t)name;
	args.buf = (ofw_cell_t)buf;
	args.buflen = length;
	ofw_call(&args);
	return args.size;
}

int
ofw_nextprop(ofw_cell_t phandle, const char* prev, void* buf)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	phandle;
		ofw_cell_t	previous;
		ofw_cell_t	buf;
		ofw_cell_t	flag;
	} args = {
		.service = (ofw_cell_t)"nextprop",
		.n_args    = 3,
		.n_returns = 1,
	};

	args.phandle = phandle;
	args.previous = (ofw_cell_t)prev;
	args.buf = (ofw_cell_t)buf;
	ofw_call(&args);
	return args.flag;
}

ofw_cell_t
ofw_instance_to_package(ofw_cell_t ihandle)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	ihandle;
		ofw_cell_t	phandle;
	} args = {
		.service = (ofw_cell_t)"instance-to-package",
		.n_args    = 1,
		.n_returns = 1,
	};

	args.ihandle = ihandle;
	ofw_call(&args);
	return args.phandle;
}

int
ofw_package_to_path(ofw_cell_t phandle, void* buffer, int buflen)
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	phandle;
		ofw_cell_t	buf;
		ofw_cell_t	buflen;
		ofw_cell_t	length;
	} args = {
		.service = (ofw_cell_t)"package-to-path",
		.n_args    = 3,
		.n_returns = 1,
	};

	args.phandle = phandle;
	args.buf = (ofw_cell_t)buffer;
	args.buflen = buflen;
	ofw_call(&args);
	return (args.length >= buflen) ? 0 : 1;
}

/* vim:set ts=2 sw=2: */
