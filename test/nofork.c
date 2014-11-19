/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "../paratec.h"

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
