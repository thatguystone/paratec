/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <string.h>
#include "err.hpp"

namespace pt
{

Err::Err(int err, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	this->vset(err, format, args);
	va_end(args);
}

Err::Err(int err, const char *format, va_list args)
{
	va_list cpy;
	va_copy(cpy, args);
	this->vset(err, format, cpy);
	va_end(cpy);
}

void Err::set(int err, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	this->vset(err, format, args);
	va_end(args);
}

void Err::vset(int err, const char *format, va_list args)
{
	if (err >= 0) {
		return;
	}

	char buff[8192];
	vsnprintf(buff, sizeof(buff), format, args);

	this->err_ = err;
	this->msg_ = buff;

	throw * this;
}

const char *Err::what() const noexcept
{
	return this->msg_.c_str();
}

OSErr::OSErr(int err,
			 std::initializer_list<int> allowed_errnos,
			 const char *format,
			 ...)
{
	int eno = errno;

	va_list args;
	va_start(args, format);
	this->vset(err, eno, std::move(allowed_errnos), format, args);
	va_end(args);
}

OSErr::OSErr(int err,
			 std::initializer_list<int> allowed_errnos,
			 const char *format,
			 va_list args)
{
	int eno = errno;

	va_list cpy;
	va_copy(cpy, args);
	this->vset(err, eno, std::move(allowed_errnos), format, cpy);
	va_end(cpy);
}

void OSErr::vset(int err,
				 int eno,
				 std::initializer_list<int> allowed_errnos,
				 const char *format,
				 va_list args)
{
	if (err >= 0) {
		return;
	}

	for (auto aeno : allowed_errnos) {
		if (aeno == eno) {
			return;
		}
	}

	char buff[8192];
	vsnprintf(buff, sizeof(buff), format, args);

	this->set(err, "%s: %s", buff, strerror(eno));
}
}
