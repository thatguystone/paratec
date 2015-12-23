/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <algorithm>
#include "results.hpp"

namespace pt
{

void Result::dump(sp<const Opts>) const
{
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

void Results::record(Result r, bool print_status)
{
	char summary = '\0';

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

	if (print_status) {
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
