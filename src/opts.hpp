/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <getopt.h>
#include <string>
#include <vector>
#include <unistd.h>
#include "err.hpp"

namespace pt
{

class Opt
{
protected:
	Opt(std::string name,
		char arg,
		std::string env,
		std::string meta_var,
		std::string help)
		: name_(name), arg_(arg), env_(env), meta_var_(meta_var), help_(help)
	{
	}

public:
	const std::string name_;
	const char arg_;
	const std::string env_;
	const std::string meta_var_;
	const std::string help_;

	virtual ~Opt() = default;

	virtual void parse(std::string val) = 0;

	virtual int argType()
	{
		return required_argument;
	}

	void showUsage();
};

template <typename T> class TypedOpt : public Opt
{
protected:
	T v_ = 0;

protected:
	TypedOpt(std::string name, char arg, std::string env, std::string help)
		: Opt(std::move(name), arg, std::move(env), "", std::move(help))
	{
	}

	TypedOpt(std::string name,
			 char arg,
			 std::string env,
			 T meta,
			 std::string help)
		: Opt(std::move(name),
			  arg,
			  std::move(env),
			  std::to_string(meta),
			  std::move(help))
	{
		this->set(meta);
	}

	TypedOpt(std::string name,
			 char arg,
			 std::string env,
			 std::string meta,
			 std::string help)
		: Opt(std::move(name),
			  arg,
			  std::move(env),
			  std::move(meta),
			  std::move(help))
	{
	}

public:
	virtual int argType() override
	{
		return required_argument;
	}

	inline T get() const
	{
		return this->v_;
	}

	inline void set(T v)
	{
		this->v_ = v;
	}

	void parse(std::string v) override;
};

class BenchOpt : public TypedOpt<bool>
{
public:
	BenchOpt() : TypedOpt<bool>("bench", 'b', "PTBENCH", "run benchmarks")
	{
	}
};

class BenchDurOpt : public TypedOpt<double>
{
public:
	BenchDurOpt()
		: TypedOpt<double>("bench-dur",
						   'd',
						   "PTBENCHDUR",
						   1.0,
						   "maximum time to run each benchmark for, in seconds")
	{
	}
};

class FilterOpt : public Opt
{
public:
	struct F {
		bool neg_;
		std::string f_;

		F(bool neg, std::string f) : neg_(neg), f_(std::move(f))
		{
		}

		const char *c_str() const
		{
			return this->f_.c_str();
		}
	};

	std::vector<F> filts_;

	FilterOpt()
		: Opt("filter",
			  'f',
			  "PTFILTER",
			  "<FILTER>...",
			  "only run tests prefixed with FILTER")
	{
	}

	void parse(std::string val) override;
};

class HelpOpt : public TypedOpt<bool>
{
public:
	HelpOpt() : TypedOpt<bool>("help", 'h', "", "print this message")
	{
	}

	void parse(std::string) override;
	void usage(const std::vector<const char *> &args,
			   const std::vector<Opt *> &opts);
};

class JobsOpt : public TypedOpt<uint>
{
public:
	JobsOpt()
		: TypedOpt<uint>("jobs",
						 'j',
						 "PTJOBS",
						 "NUMCPU",
						 "number of tests to run in parallel")
	{
		auto count = sysconf(_SC_NPROCESSORS_ONLN);
		this->v_ = (uint)std::max<decltype(count)>(1, count);
	}
};

class NoCaptureOpt : public TypedOpt<bool>
{
public:
	NoCaptureOpt()
		: TypedOpt<bool>("nocapture",
						 'n',
						 "PTNOCAPTURE",
						 "don't capture stdout/stderr")
	{
	}
};

class NoForkOpt : public TypedOpt<bool>
{
public:
	NoForkOpt()
		: TypedOpt<bool>("nofork",
						 's',
						 "PTNOFORK",
						 "run every test in a single process without "
						 "isolation, buffering, or anything else")
	{
	}
};

class PortOpt : public TypedOpt<uint16_t>
{
	static constexpr uint16_t kPort = 23120;

public:
	PortOpt()
		: TypedOpt<uint16_t>("port",
							 'p',
							 "PTPORT",
							 kPort,
							 "port number to start handing out ports at")
	{
	}
};

class TimeoutOpt : public TypedOpt<double>
{
	static constexpr double kTimeout = 5.0;

public:
	TimeoutOpt()
		: TypedOpt<double>("timeout",
						   't',
						   "PTTIMEOUT",
						   kTimeout,
						   "set the global timeout for tests")
	{
	}
};

class VerboseOpt : public TypedOpt<uint>
{
public:
	VerboseOpt()
		: TypedOpt<uint>("verbose",
						 'v',
						 "PTVERBOSE",
						 "be more verbose with the test summary; pass "
						 "multiple times to increase verbosity")
	{
	}

	void parse(std::string v) override
	{
		this->v_ += std::max((size_t)1, v.size());
	}

	int argType() override
	{
		return optional_argument;
	}

	bool passedStatuses() const
	{
		return this->v_ >= 1;
	}

	bool allStatuses() const
	{
		return this->v_ >= 2;
	}

	bool passedOutput() const
	{
		return this->v_ >= 3;
	}
};

class Opts
{
	std::vector<Opt *> getOpts();

	void tryParse(const std::vector<const char *> &args,
				  const std::vector<Opt *> &opts);

public:
	/**
	 * Name of the binary
	 */
	std::string bin_name_;

	/**
	 * If the environment is managed
	 */
	bool takeover_;

	/**
	 * !this->no_capture_.get()
	 */
	bool capture_;

	/**
	 * !this->no_fork_.get()
	 */
	bool fork_;

	BenchOpt bench_;
	BenchDurOpt bench_dur_;
	FilterOpt filter_;
	HelpOpt help_;
	JobsOpt jobs_;
	NoCaptureOpt no_capture_;
	NoForkOpt no_fork_;
	PortOpt port_;
	TimeoutOpt timeout_;
	VerboseOpt verbose_;

	/**
	 * Clear all paratec env variables to give a clean slate for testing
	 */
	void clearEnv();

	/**
	 * Expects command-line style args. That is args[0] = binary name,
	 * args[1:] = the arguments.
	 */
	void parse(std::vector<const char *> args);

	void usage(const Err &err,
			   const std::vector<const char *> &args,
			   const std::vector<Opt *> &opts)
	{
		if (err.msg_.size() > 0) {
			printf("Error: %s\n", err.msg_.c_str());
		}

		this->help_.usage(args, opts);
	}
};
}
