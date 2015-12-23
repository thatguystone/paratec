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
#include "std.hpp"

namespace pt
{

class Err : public std::exception
{
protected:
	Err() = default;

	PRINTF(3, 4)
	void set(int err, const char *format, ...);

	PRINTF(3, 0)
	void vset(int err, const char *format, va_list args);

public:
	int err_;
	std::string msg_;

	PRINTF(3, 4)
	Err(int err, const char *format, ...);

	PRINTF(3, 0)
	Err(int err, const char *format, va_list args);

	const char *what() const noexcept override;
};

class OSErr : public Err
{
	PRINTF(5, 0)
	void vset(int err,
			  int eno,
			  std::initializer_list<int> allowed_errnos,
			  const char *format,
			  va_list args);

public:
	PRINTF(4, 5)
	OSErr(int err,
		  std::initializer_list<int> allowed_errnos,
		  const char *format,
		  ...);

	PRINTF(4, 0)
	OSErr(int err,
		  std::initializer_list<int> allowed_errnos,
		  const char *format,
		  va_list args);
};
}
