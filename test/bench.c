/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <sched.h>
#include <stdio.h>
#include "../paratec.h"

PARATEC(bench, PTBENCH())
{
	uint64_t i;

	for (i = 0; i < _N; i++) {
		sched_yield();
	}
}

PARATEC(bench2, PTBENCH())
{
	uint64_t i;

	for (i = 0; i < _N; i++) {}
}

PARATEC(bench_fail, PTBENCH())
{
	pt_fail("no bench for you!");
}
