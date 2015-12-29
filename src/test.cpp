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

sp<Test> Test::bindTo(int64_t i, sp<const Opts> opts) const
{
	void *vitem = this->vec_ == nullptr ? nullptr : ((char *)this->vec_);
	auto test = mksp<Test>(static_cast<const _paratec &>(*this), i, vitem);

	test->opts_ = std::move(opts);

	for (const auto &f : test->opts_->filter_.filts_) {
		bool matches = strstr(test->name(), f.f_.c_str()) == test->name();

		if (matches) {
			test->enabled_ = f.neg_;
		}
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
