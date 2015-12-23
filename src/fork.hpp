/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <unistd.h>
#include "err.hpp"

namespace pt
{

class Fork
{
	pid_t pid_ = -1;
	int stdout_ = -1;
	int stderr_ = -1;

public:
	~Fork();

	inline pid_t pid()
	{
		return this->pid_;
	}

	inline int stdout()
	{
		return this->stdout_;
	}

	inline int stderr()
	{
		return this->stderr_;
	}

	/**
	 * Fork. Returns true if parent, false if child.
	 */
	bool fork(bool capture);

	/**
	 * Humanely kill the forked process
	 */
	void terminate();
};
}
