/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <iostream>
#include <sys/mman.h>
#include "err.hpp"
#include "std.hpp"

namespace pt
{

PT_PRINTF(2, 3)
void format(std::ostream &os, const char *format, ...);

/**
 * Call a function on scope exit.
 */
class DTor
{
	std::function<void()> cb_;

public:
	DTor(std::function<void()> cb) : cb_(std::move(cb))
	{
	}

	~DTor()
	{
		this->cb_();
	}
};

/**
 * This only works for primitive types, fixed-sized primitive arrays, and
 * structs of those.
 */
template <typename T> class SharedMem
{
	T *mem_ = nullptr;

public:
	SharedMem()
	{
		this->mem_
			= (T *)mmap(NULL, sizeof(*this->mem_), PROT_READ | PROT_WRITE,
						MAP_ANON | MAP_SHARED, -1, 0);
		OSErr(this->mem_ == nullptr ? -1 : 0, {}, "failed to mmap");
	}

	/**
	 * May not copy shared memory
	 */
	SharedMem(const SharedMem<T> &) = delete;

	/**
	 * Moving is cool, though
	 */
	SharedMem(SharedMem<T> &&o)
	{
		this->mem_ = o.mem_;
		o.mem_ = nullptr;
	}

	~SharedMem()
	{
		if (this->mem_ != nullptr) {
			munmap(this->mem_, sizeof(*this->mem_));
		}
	}

	inline T *operator->()
	{
		return this->mem_;
	}

	T *get()
	{
		return this->mem_;
	}
};
}
