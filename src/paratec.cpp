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
