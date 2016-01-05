/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <functional>
#include <stdlib.h>
#include <type_traits>
#include "paratec.h"
#include "test_env.hpp"

namespace std
{

std::string to_string(const char *t)
{
	return t;
}

std::string to_string(const std::string &t)
{
	return t;
}

std::string to_string(nullptr_t &)
{
	return "(nil)";
}
}

#define THUNK(cname, type, human, opClass, ...)                                \
	extern "C" void _pt_##cname(type expect, type got, const char *msg, ...)   \
	{                                                                          \
		__PT_FORWARD(type, type, human, opClass, type, ##__VA_ARGS__);         \
	}

#define X(name, human, opClass)                                                \
	THUNK(name, int64_t, human, opClass)                                       \
	THUNK(u##name, uint64_t, human, opClass)                                   \
	THUNK(f##name, double, human, opClass)                                     \
	THUNK(s##name, const char *, human, opClass)
__PT_FNS
#undef X

THUNK(sin, const char *, in, pt::assert::In, const char *);
THUNK(sni, const char *, not in, pt::assert::NotIn, const char *);

namespace pt
{
namespace assert
{
void _fail(const char *extra_msg, const char *msg, ...)
{
	char buff[PT_FAIL_BUFF];
	const char *delim = " :: ";

	/*
	 * extra_msg[1]: used to suppress errors about empty format strings;
	 * they're meant to be empty as a work-around to having something like
	 * pt_eq() and pt_eq_msg().
	 */
	if (extra_msg[0] == '\0' || extra_msg[1] == '\0') {
		extra_msg = delim = "";
	}

	// Skip that workaround space, if present
	if (extra_msg[0] == ' ') {
		extra_msg++;
	}

	va_list args;
	va_start(args, msg);
	vsnprintf(buff, sizeof(buff), msg, args);
	va_end(args);

	_pt_fail("%s%s%s", buff, delim, extra_msg);
}
}
}

extern "C" {
void _pt_ner(const ssize_t got, const int eno, const char *msg, ...)
{
	if (got == -1) {
		va_list args;
		char buff[PT_FAIL_BUFF];

		va_start(args, msg);
		vsnprintf(buff, sizeof(buff), msg, args);
		va_end(args);

		pt::assert::_fail(buff, "Expected no error, got: (%d) %s", eno,
						  strerror(eno));
	}
}
}
