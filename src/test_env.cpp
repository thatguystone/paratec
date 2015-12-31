/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <string.h>
#include "test_env.hpp"

namespace pt
{

void TestEnv::reset(uint id, const char *test_name, const char *func_name)
{
	this->id_ = id;
	this->failed_ = false;
	this->skipped_ = false;
	this->iter_name_[0] = '\0';
	this->last_mark_[0] = '\0';
	this->fail_msg_[0] = '\0';

	strncpy(this->test_name_, test_name, sizeof(this->test_name_));
	strncpy(this->func_name_, func_name, sizeof(this->func_name_));
	strncpy(this->last_test_mark_, "test start", sizeof(this->last_test_mark_));
}
}
