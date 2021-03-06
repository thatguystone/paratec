/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
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

bool Fork::flush(int fd, std::string *s)
{
	ssize_t err;
	char buff[4096];

	do {
		err = read(fd, buff, sizeof(buff));
		OSErr(err, { EAGAIN, EWOULDBLOCK }, "failed to read from subprocess");

		if (err > 0) {
			s->append(buff, (size_t)err);
		}
	} while (err == sizeof(buff));

	return err != 0;
}

bool Fork::flushPipes()
{
	bool flushed = false;

	if (this->stdout_ != -1) {
		flushed = this->flush(this->stdout_, &this->out_);
		flushed |= this->flush(this->stderr_, &this->err_);
	}

	return flushed;
}

bool Fork::fork(bool capture, bool newpgid)
{
	int err;
	_Pipes pipes(capture);

	this->pid_ = ::fork();
	OSErr(this->pid_, {}, "failed to fork");

	if (this->pid_ == 0) {
		sig::reset();

		if (newpgid) {
			err = setpgid(0, 0);
			OSErr(err, {}, "could not setpgid");
		}

		if (capture) {
			pipes.setChild();
		}

		return false;
	}

	if (capture) {
		pipes.getParentEnds(&this->stdout_, &this->stderr_);
	}

	if (newpgid) {
		// There's a race condition here: calling terminate() before setpgid()
		// is called does nothing. So wait until the process either (1) dies
		// or (2) sets it pgid.
		do {
			usleep(100);
			err = getpgid(this->pid_);
		} while (err != -1 && err != this->pid_);
	}

	return true;
}

Fork::Exit Fork::run(std::function<void()> fn)
{
	int status;
	bool parent = this->fork(true, false);

	if (!parent) {
		fn();
		exit(0);
	}

	auto closed = false;
	while (!closed) {
		pollfd pfds[] = {
			{
			 .fd = this->stdout_, .events = POLLIN, .revents = 0,
			},
			{
			 .fd = this->stderr_, .events = POLLIN, .revents = 0,
			},
		};

		auto err = poll(pfds, 2, -1);
		OSErr(err, {}, "failed to poll child pipes");

		closed = !this->flushPipes();
	}

	auto err = waitpid(this->pid_, &status, 0);
	OSErr(err, {}, "failed to reap child");

	this->flushPipes();

	Fork::Exit e;
	e.status_ = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
	e.signal_ = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
	e.stdout_ = std::move(this->out_);
	e.stderr_ = std::move(this->err_);

	return std::move(e);
}

int Fork::terminate(int *status)
{
	int i;
	int err;
	int status_;

	if (status == nullptr) {
		status = &status_;
	}

	killpg(this->pid_, SIGTERM);

	for (i = 0; i < 100; i++) {
		err = waitpid(this->pid_, status, WNOHANG);
		if (err != 0 && (WIFEXITED(*status) || WIFSIGNALED(*status))) {
			return err;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// If the process doesn't end, forcibly terminate
	killpg(this->pid_, SIGKILL);
	return waitpid(this->pid_, status, 0);
}
}
