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
#include "util.hpp"

namespace pt
{

#define STDPREFIX INDENT INDENT INDENT " | "

void Result::reset(sp<const Test> test)
{
	*this = Result();
	this->test_ = std::move(test);
	this->start_ = time::now();
}

void Result::dumpOuts(std::ostream &os, bool print) const
{
	if (!print) {
		return;
	}

	this->dumpOut(os, "stdout", this->stdout_);
	this->dumpOut(os, "stderr", this->stderr_);

	if (this->stdout_.size() > 0 || this->stderr_.size() > 0) {
		format(os, "\n");
	}
}

void Result::dumpOut(std::ostream &os,
					 const char *which,
					 const std::string &s) const
{
	if (s.size() == 0) {
		return;
	}

	format(os, INDENT INDENT INDENT "%s\n", which);

	std::string l;
	std::istringstream is(s);
	while (std::getline(is, l)) {
		os.write(STDPREFIX, strlen(STDPREFIX));
		os.write(l.c_str(), l.size());
		os.put('\n');
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
	} else if (te.failed_) {
		this->failed_ = true;
	} else if (this->signal_num_ != 0 || this->test_->signal_num_ != 0) {
		this->error_ = this->test_->signal_num_ != this->signal_num_;
	} else if (this->exit_status_ != 0 || this->test_->exit_status_ != 0) {
		this->error_ = this->test_->exit_status_ != this->exit_status_;
	} else if (te.failed_ && !this->test_->expect_fail_) {
		this->failed_ = true;
	}

	auto passed = this->skipped_
				  || (!this->failed_ && !this->error_ && !this->timedout_);
	if (passed && !v.passedOutput()) {
		this->stdout_.clear();
		this->stderr_.clear();
	}

	if (!passed) {
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

void Result::dump(std::ostream &os, sp<const Opts> opts) const
{
	const auto &v = opts->verbose_;

	if (!this->enabled()) {
		if (v.allStatuses()) {
			format(os, INDENT "DISABLED : %s \n", this->name_.c_str());
		}
		return;
	}

	if (this->skipped_) {
		if (v.allStatuses()) {
			format(os, INDENT "    SKIP : %s \n", this->name_.c_str());
		}
		this->dumpOuts(os, v.passedOutput());
		return;
	}

	if (this->error_) {
		format(os, INDENT "   ERROR : %s (%fs) : after %s : ",
			   this->name_.c_str(), this->duration_, this->last_line_.c_str());

		if (this->signal_num_ != 0 || this->test_->signal_num_ != 0) {
			format(os, "received signal (%d) `%s`, expected (%d) `%s`\n",
				   this->signal_num_, strsignal(this->signal_num_),
				   this->test_->signal_num_,
				   strsignal(this->test_->signal_num_));
		} else {
			format(os, "got exit code=%d, expected %d\n", this->exit_status_,
				   this->test_->exit_status_);
		}

		this->dumpOuts(os, true);
		return;
	}

	if (this->failed_) {
		format(os, INDENT "    FAIL : %s (%fs) : %s : %s\n",
			   this->name_.c_str(), this->duration_, this->last_line_.c_str(),
			   this->fail_msg_.c_str());
		this->dumpOuts(os, true);
		return;
	}

	if (this->timedout_) {
		format(os, INDENT "TIME OUT : %s (%fs) : after %s\n",
			   this->name_.c_str(), this->duration_, this->last_line_.c_str());
		this->dumpOuts(os, true);
		return;
	}

	if (this->test_->bench_) {
		format(os, INDENT "   BENCH : %s (%'" PRIu64 " @ %'" PRIu64 " ns/op)\n",
			   this->name_.c_str(), this->bench_iters_, this->bench_ns_op_);
		this->dumpOuts(os, v.passedOutput());
		return;
	}

	if (v.passedStatuses()) {
		format(os, INDENT "    PASS : %s (%fs) \n", this->name_.c_str(),
			   this->duration_);
	}
	this->dumpOuts(os, v.passedOutput());
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

	if (!r.enabled()) {
		// Skip all tallying
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
	} else {
		summary = '.';
		this->passes_++;
	}

	this->results_.push_back(std::move(r));

	if (this->opts_->fork_ && this->opts_->capture_) {
		if (summary != '\0') {
			format(this->os_, "%c", summary);
			this->os_.flush();
		}

		if (this->done()) {
			format(this->os_, "\n");
		}
	}

	if (this->done()) {
		this->end_ = time::now();
	}
}

Result Results::get(const std::string &name)
{
	for (const auto &r : this->results_) {
		if (r.name_ == name) {
			return r;
		}
	}

	throw Err(-1, "result for %s not found", name.c_str());
}

void Results::dump()
{
	std::sort(this->results_.begin(), this->results_.end());

	format(this->os_, "%d%%: ",
		   this->enabled_ == 0 ? 100 : (int)(((double)this->passes_
											  / this->enabled_) * 100));
	format(this->os_, "of %zu tests run, ", this->enabled_);
	format(this->os_, "%zu OK, ", this->passes_);
	format(this->os_, "%zu errors, ", this->errors_);
	format(this->os_, "%zu failures, ", this->failures_);
	format(this->os_, "%zu skipped. ", this->skipped_);
	format(this->os_, "Ran in %fs (tests used %fs)\n",
		   time::toSeconds(this->end_ - this->start_), this->tests_duration_);

	for (const auto &r : this->results_) {
		r.dump(this->os_, this->opts_);
	}
}
}
