/* Implements commands as described in 6.3.2.3: device tree */
#include <sys/lib.h> 
#include <ofw.h> 

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
	ofw_entry(&args);
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
	ofw_entry(&args);
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
	ofw_entry(&args);
	return args.size;
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
	ofw_entry(&args);
	return args.phandle;
}

/* vim:set ts=2 sw=2: */
