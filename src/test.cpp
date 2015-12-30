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

sp<const Test> Test::bindTo(int64_t i, sp<const Opts> opts) const
{
	void *vitem = this->vec_ == nullptr ? nullptr : ((char *)this->vec_);
	auto test = mksp<Test>(static_cast<const _paratec &>(*this), i, vitem);

	test->opts_ = std::move(opts);
	const auto &filts = test->opts_->filter_.filts_;

	// By default, not disabled. Once matched in a negative filter, however,
	// can't be re-enabled.
	bool disabled = false;

	// If there are no filters, everything is enabled.
	bool enabled = filts.size() == 0;

	// Every test becomes disabled by default if a single positive filter is
	// given. Only things that pass the filter run.
	bool hasPosFilt = false;

	for (const auto &f : filts) {
		bool matches = strstr(test->name(), f.c_str()) == test->name();

		hasPosFilt |= !f.neg_;
		if (matches) {
			disabled |= f.neg_;
			enabled |= !f.neg_;
		}
	}

	if (hasPosFilt) {
		test->enabled_ = !disabled && enabled;
	} else {
		test->enabled_ = !disabled;
	}

	return test;
}

void Test::run() const
{
	if (this->setup_ != NULL) {
		this->setup_();
	}

	this->fn_(this->i_, 0, this->vitem_);

	if (this->teardown_ != NULL) {
		this->teardown_();
	}
}

void Test::cleanup() const
{
	if (this->cleanup_ != nullptr) {
		this->cleanup_();
	}
}
}
