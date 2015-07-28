/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "../paratec.h"

static void _mark(void)
{
	pt_mark();
}

static void _fail(void)
{
	pt_fail("here");
}

PARATEC(nofork)
{
	pt_seq("a", "a");
	pt_sne("a", "b");

	pt_meq("a", "a", 1);
	pt_mne("a", "b", 1);
}

PARATEC(nofork_successful_failure, PTFAIL())
{
	pt_seq("a", "b");
}

PARATEC(nofork_failure)
{
	pt_seq("a", "b");
}

PARATEC(nofork_mark)
{
	_mark();
	pt_seq("a", "b");
}

PARATEC(nofork_fail_outside)
{
	_fail();
}

PARATEC(nofork_fail_outside_marked)
{
	pt_mark();
	_fail();
}
