/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <fcntl.h>
#include <sys/wait.h>
#include "fork.hpp"
#include "signal.hpp"

namespace pt
{

class _Pipes
{
	int fds_[3][2]{
		{ -1, -1 },
		{ -1, -1 },
		{ -1, -1 },
	};

	void dup(int std, int end)
	{
		int err;
		int from = this->fds_[std][end];

		err = dup2(from, std);
		OSErr(err, {}, "failed to dup2");

		close(from);
	}

public:
	_Pipes(bool create)
	{
		int i;
		int err;

		if (!create) {
			return;
		}

		for (i = 0; i < 3; i++) {
			err = pipe(this->fds_[i]);
			OSErr(err, {}, "failed to create pipes");

			err = fcntl(this->fds_[i][0], F_SETFL, O_NONBLOCK);
			OSErr(err, {}, "failed to set test pipe nonblocking");
		}
	}

	void setChild()
	{
		close(this->fds_[STDIN_FILENO][1]);
		close(this->fds_[STDOUT_FILENO][0]);
		close(this->fds_[STDERR_FILENO][0]);

		this->dup(STDIN_FILENO, 0);
		this->dup(STDOUT_FILENO, 1);
		this->dup(STDERR_FILENO, 1);

		setlinebuf(stdout);
		setlinebuf(stderr);
	}

	void getParentEnds(int *stdout, int *stderr)
	{
		int i;

		close(this->fds_[STDIN_FILENO][0]);
		for (i = 0; i < 3; i++) {
			close(this->fds_[i][1]);
		}

		*stdout = this->fds_[STDOUT_FILENO][0];
		*stderr = this->fds_[STDERR_FILENO][0];
	}
};

Fork::~Fork()
{
	if (this->stdout_ != -1) {
		close(this->stdout_);
		close(this->stderr_);
	}
}

bool Fork::fork(bool capture)
{
	int err;
	_Pipes pipes(capture);

	this->pid_ = ::fork();
	OSErr(this->pid_, {}, "failed to fork");

	if (this->pid_ == 0) {
		sig::reset();

		if (capture) {
			pipes.setChild();
		}

		err = setpgid(0, 0);
		OSErr(err, {}, "could not setpgid");

		return false;
	}

	if (capture) {
		pipes.getParentEnds(&this->stdout_, &this->stderr_);
	}

	return true;
}

void Fork::terminate()
{
	// Send SIGINT first to give the test a chance to cleanup, but be sure
	// he's dead anyway. SIGKILL to the rescue!
	killpg(this->pid_, SIGINT);
	killpg(this->pid_, SIGKILL);
}
}
