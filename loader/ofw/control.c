/* Implements commands as described in 6.3.2.5: control transfer */
#include <loader/lib.h>
#include <ofw.h>

void
ofw_enter()
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
	} args = {
		.service = (ofw_cell_t)"enter",
		.n_args    = 0,
		.n_returns = 0,
	};

	ofw_entry(&args);
}

void
ofw_exit()
{
	struct {
		ofw_cell_t	service;
		ofw_cell_t	n_args;
		ofw_cell_t	n_returns;
	} args = {
		.service = (ofw_cell_t)"exit",
		.n_args    = 0,
		.n_returns = 0,
	};

	ofw_entry(&args);
}


/* vim:set ts=2 sw=2: */
