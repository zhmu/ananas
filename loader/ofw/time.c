#include <loader/lib.h>
#include <ofw.h>

uint32_t
ofw_milliseconds()
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
		ofw_cell_t	ms;
	} args = {
		.service = (ofw_cell_t)"milliseconds",
		.n_args    = 0,
		.n_returns = 1,
	};

	ofw_entry(&args);
	return args.ms;
}

/* vim:set ts=2 sw=2: */
