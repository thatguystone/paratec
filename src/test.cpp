/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "test.hpp"

namespace pt
{

void Test::clean()
{
	if (this->cleanup != nullptr) {
		this->cleanup();
	}
}

void Test::flush(int fd, std::string *to)
{
	ssize_t err;
	char buff[4096];

	do {
		err = read(fd, buff, sizeof(buff));
		OSErr(err, { EAGAIN, EWOULDBLOCK }, "failed to read from subprocess");

		if (err > 0) {
			to->append(buff, err);
		}
	} while (err == sizeof(buff));
}

void Test::prepare(sp<const Opts> opts, sp<Results> res)
{
	this->opts_ = std::move(opts);
	this->res_ = std::move(res);

	for (const auto &f : this->opts_->filter_.filts_) {
		bool matches = strstr(this->name, f.f_.c_str()) == this->name;

		if (matches) {
			this->enabled_ = f.neg_;
		}
	}

	this->res_->inc(this->enabled());
}

void Test::run()
{
	if (!this->enabled()) {
		Result r(this->name);
		r.disabled_ = true;
		this->res_->record(std::move(r), false);
		return;
	}
}

void Test::flushPipes(int stdout, int stderr)
{
	this->flush(stdout, &this->stdout_);
	this->flush(stderr, &this->stderr_);
}

void Test::cleanupFork(time::point start,
					   int status,
					   bool timedout,
					   bool assertsOK)
{
	Result r(this->name, start);
	bool passed = false;

	if (WIFEXITED(status)) {
		r.exit_status_ = WEXITSTATUS(status);
		passed = r.exit_status_ == this->exit_status;
	} else if (WIFSIGNALED(status)) {
		r.signal_num_ = WTERMSIG(status);
		passed = r.signal_num_ == this->signal_num;
	}

	if (this->skipped_) {
		r.skipped_ = true;
	} else if (timedout) {
		r.timedout_ = true;
	} else if (passed || (!assertsOK && this->expect_fail)) {
		r.passed_ = true;
	} else if (!assertsOK) {
		r.failed_ = true;
	} else {
		r.error_ = true;
	}

	this->res_->record(std::move(r), this->opts_->capture_);
}

void Test::cleanupNoFork(time::point, bool)
{
}
}
