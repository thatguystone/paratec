/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "err.hpp"
#include "util.hpp"

namespace pt
{

void format(std::ostream &os, const char *format, ...)
{
	int actual;
	char buff[4096];

	{
		va_list args;
		va_start(args, format);
		actual = vsnprintf(buff, sizeof(buff), format, args);
		OSErr(actual, {}, "failed to format output");
		va_end(args);
	}

	if (((size_t)actual) < sizeof(buff)) {
		os << buff;
	} else {
		char bbuff[actual + 1];

		va_list args;
		va_start(args, format);
		vsnprintf(bbuff, sizeof(bbuff), format, args);
		va_end(args);

		os << bbuff;
	}
}
}
