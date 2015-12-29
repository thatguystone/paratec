/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <algorithm>
#include <inttypes.h>
#include <sstream>
#include <string.h>
#include <string>
#include "results.hpp"

namespace pt
{

#define STDPREFIX INDENT INDENT INDENT " | "

void Result::reset(sp<const Test> test)
{
	*this = Result();
	this->test_ = std::move(test);
	this->start_ = time::now();
}

void Result::dumpOuts(bool print) const
{
	if (!print) {
		return;
	}

	this->dumpOut("stdout", this->stdout_);
	this->dumpOut("stderr", this->stderr_);

	if (this->stdout_.size() > 0 || this->stderr_.size() > 0) {
		printf("\n");
	}
}

void Result::dumpOut(const char *which, const std::string &s) const
{
	if (s.size() == 0) {
		return;
	}

	printf(INDENT INDENT INDENT "%s\n", which);

	std::string l;
	std::istringstream is(s);
	while (std::getline(is, l)) {
		fwrite(STDPREFIX, strlen(STDPREFIX), 1, stdout);
		fwrite(l.c_str(), l.size(), 1, stdout);
		fwrite("\n", 1, 1, stdout);
	}
}

void Result::finalize(const TestEnv &te, sp<const Opts> opts)
{
	const auto &v = opts->verbose_;

	this->name_ = te.test_name_;

	if (te.skipped_) {
		this->skipped_ = true;
	} else if (this->timedout_) {
		// Skip so that the fallthrough isn't hit
	} else if (!te.failed_ || (te.failed_ && this->test_->expect_fail_)) {
		this->passed_ = true;
	} else if (te.failed_) {
		this->failed_ = true;
	} else {
		this->error_ = true;
	}

	if (this->passed_ && !v.passedOutput()) {
		this->stdout_.clear();
		this->stderr_.clear();
	}

	if (!this->passed_) {
		this->fail_msg_ = te.fail_msg_;

		if (*te.last_mark_ != '\0') {
			char buff[sizeof(te.last_mark_) * 2];
			snprintf(buff, sizeof(buff), "%s (last test assert: %s)",
					 te.last_mark_, te.last_test_mark_);
			this->last_line_ = buff;
		} else {
			this->last_line_ = te.last_test_mark_;
		}
	}
}

void Result::dump(sp<const Opts> opts) const
{
	const auto &v = opts->verbose_;

	if (!this->test_->enabled()) {
		if (v.allStatuses()) {
			printf(INDENT "DISABLED : %s \n", this->name_.c_str());
		}
		return;
	}

	if (this->passed_) {
		if (v.allStatuses()) {
			printf(INDENT "    PASS : %s (%fs) \n", this->name_.c_str(),
				   this->duration_);
		}
		this->dumpOuts(v.passedOutput());
		return;
	}

	if (this->skipped_) {
		if (v.allStatuses()) {
			printf(INDENT "    SKIP : %s \n", this->name_.c_str());
		}
		this->dumpOuts(v.passedOutput());
		return;
	}

	if (this->error_) {
		printf(INDENT "   ERROR : %s (%fs) : after %s : ", this->name_.c_str(),
			   this->duration_, this->last_line_.c_str());

		if (this->signal_num_ != 0) {
			printf("received signal(%d) `%s`\n", this->signal_num_,
				   strsignal(this->signal_num_));
		} else {
			printf("exit code=%d\n", this->exit_status_);
		}

		this->dumpOuts(true);
		return;
	}

	if (this->failed_) {
		printf(INDENT "    FAIL : %s (%fs) : %s : %s\n", this->name_.c_str(),
			   this->duration_, this->last_line_.c_str(),
			   this->fail_msg_.c_str());
		this->dumpOuts(true);
		return;
	}

	if (this->timedout_) {
		printf(INDENT "TIME OUT : %s (%fs) : after %s\n", this->name_.c_str(),
			   this->duration_, this->last_line_.c_str());
		this->dumpOuts(true);
		return;
	}

	if (this->test_->bench_) {
		printf(INDENT "   BENCH : %s (%'" PRIu64 " @ %'" PRIu64 " ns/op)\n",
			   this->name_.c_str(), this->bench_iters_, this->bench_ns_op_);
		this->dumpOuts(v.passedOutput());
		return;
	}
}

void Results::startTimer()
{
	this->start_ = time::now();
}

void Results::inc(bool enabled)
{
	this->total_++;
	if (enabled) {
		this->enabled_++;
	}
}

void Results::record(const TestEnv &ti, Result r)
{
	char summary = '\0';

	r.finalize(ti, this->opts_);

	this->finished_++;
	this->tests_duration_ += r.duration_;

	if (r.passed_) {
		summary = '.';
		this->passes_++;
	} else if (r.skipped_) {
		summary = 'S';
		this->skipped_++;
	} else if (r.error_) {
		summary = 'E';
		this->errors_++;
	} else if (r.failed_) {
		summary = 'F';
		this->failures_++;
	} else if (r.timedout_) {
		summary = 'T';
		this->failures_++;
	}

	this->results_.push_back(std::move(r));

	if (this->opts_->fork_ && this->opts_->capture_) {
		printf("%c", summary);
		fflush(stdout);

		if (this->done()) {
			printf("\n");
		}
	}

	if (this->done()) {
		this->end_ = time::now();
	}
}

void Results::dump()
{
	std::sort(this->results_.begin(), this->results_.end());

	printf("%d%%: ",
		   this->enabled_ == 0 ? 100 : (int)(((double)this->passes_
											  / this->enabled_) * 100));
	printf("of %zu tests run, ", this->enabled_);
	printf("%zu OK, ", this->passes_);
	printf("%zu errors, ", this->errors_);
	printf("%zu failures, ", this->failures_);
	printf("%zu skipped. ", this->skipped_);
	printf("Ran in %fs (tests used %fs)\n",
		   time::toSeconds(this->end_ - this->start_), this->tests_duration_);

	for (const auto &r : this->results_) {
		r.dump(this->opts_);
	}
}
}
