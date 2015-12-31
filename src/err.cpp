/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <errno.h>
#include <string.h>
#include "err.hpp"
#include "util.hpp"

namespace pt
{

Err::Err(ssize_t err, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	DTor d([&]() { va_end(args); });

	this->vset(err, format, args);
}

Err::Err(ssize_t err, const char *format, va_list args)
{
	this->vset(err, format, args);
}

void Err::set(ssize_t err, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	DTor d([&]() { va_end(args); });

	this->vset(err, format, args);
}

void Err::vset(ssize_t err, const char *format, va_list args)
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

OSErr::OSErr(ssize_t err,
			 std::initializer_list<int> allowed_errnos,
			 const char *format,
			 ...)
{
	int eno = errno;

	va_list args;
	va_start(args, format);
	DTor d([&]() { va_end(args); });

	this->vset(err, eno, std::move(allowed_errnos), format, args);
}

OSErr::OSErr(ssize_t err,
			 std::initializer_list<int> allowed_errnos,
			 const char *format,
			 va_list args)
{
	int eno = errno;
	this->vset(err, eno, std::move(allowed_errnos), format, args);
}

void OSErr::vset(ssize_t err,
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
