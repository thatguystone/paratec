/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <exception>
#include <stdarg.h>
#include <string>
#include <vector>
#include "paratec.h"
#include "std.hpp"

namespace pt
{

class Err : public std::exception
{
protected:
	Err() = default;

	PT_PRINTF(3, 4)
	void set(ssize_t err, const char *format, ...);

	PT_PRINTF(3, 0)
	void vset(ssize_t err, const char *format, va_list args);

public:
	ssize_t err_;
	std::string msg_;

	PT_PRINTF(3, 4)
	Err(ssize_t err, const char *format, ...);

	PT_PRINTF(3, 0)
	Err(ssize_t err, const char *format, va_list args);

	const char *what() const noexcept override;
};

class OSErr : public Err
{
	PT_PRINTF(5, 0)
	void vset(ssize_t err,
			  int eno,
			  std::initializer_list<int> allowed_errnos,
			  const char *format,
			  va_list args);

public:
	PT_PRINTF(4, 5)
	OSErr(ssize_t err,
		  std::initializer_list<int> allowed_errnos,
		  const char *format,
		  ...);

	PT_PRINTF(4, 0)
	OSErr(ssize_t err,
		  std::initializer_list<int> allowed_errnos,
		  const char *format,
		  va_list args);
};
}
