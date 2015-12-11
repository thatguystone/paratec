/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "../paratec.h"

struct table {
	int a;
	int b;
	int c;
};

static struct table _table[] = {
	{
	 .a = 0,
	 .b = 0,
	 .c = 0,
	},
	{
	 .a = 1,
	 .b = 1,
	 .c = 1,
	},
	{
	 .a = 2,
	 .b = 2,
	 .c = 2,
	},
};

PARATECV(table_test, _table)
{
	pt_eq(_t->a, _i);
	pt_eq(_t->b, _i);
	pt_eq(_t->c, _i);
}
