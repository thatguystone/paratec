/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <functional>
#include <string>
#include <tuple>
#include <unistd.h>
#include "err.hpp"

namespace pt
{

class Fork
{
	pid_t pid_ = -1;
	int stdout_ = -1;
	int stderr_ = -1;

	std::string out_;
	std::string err_;

	bool flush(int fd, std::string *s);

public:
	struct Exit {
		int status_;
		int signal_;
		std::string stdout_;
		std::string stderr_;
	};

	Fork() = default;
	Fork(const Fork &) = delete;
	Fork(Fork &&) = delete;
	~Fork();

	inline pid_t pid() const
	{
		return this->pid_;
	}

	inline int stdout() const
	{
		return this->stdout_;
	}

	inline int stderr() const
	{
		return this->stderr_;
	}

	inline void moveOuts(std::string *out, std::string *err)
	{
		*out = std::move(this->out_);
		*err = std::move(this->err_);
	}

	/**
	 * Flush the underlying pipes. Returns true if the pipes are still open
	 * (aka. process is still running), false if closed (aka. process exited).
	 */
	bool flushPipes();

	/**
	 * Fork. Returns true if parent, false if child.
	 */
	bool fork(bool capture, bool newpgid);

	/**
	 * Run the given function in the forked process, and wait for the process
	 * to exit.
	 */
	Exit run(std::function<void()> fn);

	/**
	 * Kill the forked process.
	 *
	 * @return
	 *     Output from waitpid.
	 */
	int terminate(int *status);
};
}
