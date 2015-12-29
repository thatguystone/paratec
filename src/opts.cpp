/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdexcept>
#include <string>
#include <stdlib.h>
#include <string.h>
#include "err.hpp"
#include "opts.hpp"
#include "std.hpp"

namespace pt
{

void Opt::showUsage()
{
	std::string metavar;

	if (this->meta_var_.size() > 0) {
		metavar = "=" + this->meta_var_;
	}

	printf(INDENT "-%c%s, --%s%s\n", this->arg_, metavar.c_str(),
		   this->name_.c_str(), metavar.c_str());
	printf(INDENT INDENT "%s\n", this->help_.c_str());
}

template <> void TypedOpt<int>::parse(std::string arg)
{
	try {
		this->set(std::stoi(arg));
	} catch (std::invalid_argument) {
		Err(-1, "`%s` could not be parsed to an integer", arg.c_str());
	} catch (std::out_of_range) {
		Err(-1, "`%s` is too large to be an integer", arg.c_str());
	}
}

template <> void TypedOpt<double>::parse(std::string arg)
{
	try {
		this->set(std::stod(arg));

		if (this->get() < 0) {
			Err(-1, "`%s` must not be less than 0", arg.c_str());
		}
	} catch (std::invalid_argument) {
		Err(-1, "`%s` could not be parsed to a double", arg.c_str());
	} catch (std::out_of_range) {
		Err(-1, "`%s` is too large to be a double", arg.c_str());
	}
}

template <> void TypedOpt<bool>::parse(std::string)
{
	this->set(true);
}

template <> int TypedOpt<bool>::argType()
{
	return no_argument;
}

void FilterOpt::parse(std::string args)
{
	char in[args.size() + 1];
	char *ary = in;

	strcpy(in, args.c_str());

	while (true) {
		char *f = strtok(ary, ", ");
		ary = nullptr;

		if (f == nullptr) {
			break;
		}

		bool neg = *f == '-';
		if (neg) {
			f++;
		}

		this->filts_.emplace_back(neg, f);
	}
}

void HelpOpt::parse(std::string)
{
	Err(-1, "");
}

void HelpOpt::usage(const std::vector<const char *> &args,
					const std::vector<Opt *> &opts)
{
	printf("\n");
	printf("Usage: %s [OPTION]...\n", args[0]);
	printf("\n");

	for (auto opt : opts) {
		opt->showUsage();
	}

	exit(1);
}

std::vector<Opt *> Opts::getOpts()
{
	return {
		&this->bench_,   &this->bench_dur_,  &this->filter_,  &this->help_,
		&this->jobs_,	&this->no_capture_, &this->no_fork_, &this->port_,
		&this->timeout_, &this->verbose_,
	};
}

void Opts::clearEnv()
{
	auto opts = this->getOpts();

	for (const auto &opt : opts) {
		unsetenv(opt->env_.c_str());
	}
}

void Opts::parse(std::vector<const char *> args)
{
	auto opts = this->getOpts();

	try {
		this->tryParse(args, opts);
		this->capture_ = !this->no_capture_.get();
		this->fork_ = !this->no_fork_.get();
	} catch (Err err) {
		this->usage(err, args, opts);
	}
}

void Opts::tryParse(const std::vector<const char *> &args,
					const std::vector<Opt *> &opts)
{
	int i;
	std::string optstr;

	this->bin_name_ = args[0];

	struct option lopts[opts.size() + 1];
	memset(lopts, 0, sizeof(lopts));

	i = 0;
	for (auto opt : opts) {
		auto lopt = &lopts[i++];

		lopt->name = opt->name_.c_str();
		lopt->val = opt->arg_;

		optstr += opt->arg_;

		lopt->has_arg = opt->argType();
		switch (lopt->has_arg) {
		case required_argument:
			optstr += ':';
			break;

		case optional_argument:
			optstr += "::";
			break;
		}

		char *v = getenv(opt->env_.c_str());
		if (v != NULL) {
			opt->parse(v);
		}
	}

	optind = 1;
	while (1) {
		char c = getopt_long(args.size(), (char *const *)args.data(),
							 optstr.c_str(), lopts, NULL);
		if (c == -1) {
			break;
		}

		bool found = false;
		for (auto opt : opts) {
			if (opt->arg_ != c) {
				continue;
			}

			opt->parse(optarg ?: "");

			found = true;
			break;
		}

		if (!found) {
			Err(-1, "");
		}
	}
}
}
