/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "paratec.h"
#include <stdlib.h>

PARATEC(assertsBasic)
{
	pt_eq(0, 0);
	pt_eq('a', 'a');
	pt_eq("test", "test");

	pt_ne(0, 1);
	pt_ne('a', 'A');
	pt_ne("test", "TEST");

	pt_gt(1, 0);
	pt_gt("z", "a");

	pt_ge(1, 1);
	pt_ge("z", "z");

	pt_lt(0, 1);
	pt_lt("a", "z");

	pt_le(1, 1);
	pt_le("z", "z");

	pt_in("cde", "abcdefgh");
	pt_ni("xyz", "abcdefgh");
}

PARATEC(assertsMem)
{
	char a[16];
	char b[32];

	memset(a, 0, sizeof(a));
	memset(b, 0, sizeof(b));

	pt_meq(a, b, sizeof(a), "extra halp");

	memset(b, 1, sizeof(b));
	pt_mne(a, b, sizeof(a), "extra halp");
}

PARATEC(assertsMemFail, PTFAIL())
{
	char a[16];
	char b[32];

	memset(a, 0, sizeof(a));
	memset(b, 1, sizeof(b));
	pt_meq(a, b, sizeof(a), "extra halp");
}

PARATEC(assertsErrno)
{
	pt_ner(0);
	pt_ner(0, "that's not an error");
}

PARATEC(assertsErrnoFail, PTFAIL())
{
	errno = EAGAIN;
	pt_ner(-1, "oh noes, it failz!");
}

PARATEC(assertsFailure, PTFAIL())
{
	pt_eq(0, 1);
}

PARATEC(waitFor)
{
	int i = 0;
	pt_wait_for(i++ == 10);
}
