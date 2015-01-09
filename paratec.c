/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <locale.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "paratec.h"
#ifdef PT_DARWIN
	#include <mach/mach_time.h>
#endif

#define MAX_BENCH_ITERS 1000000000
#define SLEEP_TIME (10 * 1000)
#define PORT 43120
#define FAIL_EXIT_STATUS 255
#define INDENT "    "
#define STDPREFIX INDENT INDENT " | "
#define LINE_SIZE 2048
#define FAILMSG_SIZE 8192

#define RUNTIME(start) ((_unow() - start) / ((double)(1000 * 1000)))
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

struct bench {
	uint64_t iters; // How many iterations ran on the final call
	uint64_t ns_op; // Time spent on each iteration
};

struct job {
	uint32_t id;
	pid_t pid;
	int stdout;
	int stderr;
	uint32_t i; // Index of the test in tests.all
	int64_t start;
	int64_t end_at;
	int timed_out;
	int skipped;
	struct bench bench;
	char iter_name[LINE_SIZE / 2];
	char fn_name[LINE_SIZE]; // Paratec's function name for this test case
	char test_name[LINE_SIZE]; // Print-friendly name for test case
	char last_line[LINE_SIZE]; // Last line anywhere in the program that ran
	char last_fn_line[LINE_SIZE]; // Last line in the test case that ran
	char fail_msg[FAILMSG_SIZE];
};

struct buff {
	char *str;
	size_t len;
	size_t alloc;
};

struct test {
	char name[LINE_SIZE];
	int exit_status;
	int signal_num;
	double duration;
	int64_t i; // For ranged tests, the i in the range
	void *titem; // For table-tests, the item in the table
	struct buff stdout;
	struct buff stderr;
	char last_line[LINE_SIZE * 2];
	char fail_msg[FAILMSG_SIZE];
	struct bench bench;
	struct paratec *p;
	struct {
		int run:1;
		int filtered_run:1;
		int passed:1;
		int timed_out:1;
		int is_ranged:1;
	} flags;
};

struct tests {
	uint32_t passes;
	uint32_t errors;
	uint32_t failures;

	uint32_t c;
	uint32_t allocated;
	uint32_t enabled;
	uint32_t next_test;
	struct test *all;
};

static int _bench;
static uint32_t _bench_duration;
static int _exit_fast;
static uint32_t _max_jobs;
static int _nofork;
static int _nocapture;
static uint32_t _port;
static uint32_t _timeout;
static int _verbose;

// Only for use in testing processes
static struct job *_tjob;
static jmp_buf _tfail;

// Only for use in the parent process
static struct job *_jobs;
static uint32_t _jobsc;

// When not forking, protect the longjmp!
static pthread_t _pthself;

static __attribute((noreturn)) void _exit_test(int status)
{
	if (_nofork) {
		longjmp(_tfail, 1);
	}

	if (_exit_fast) {
		_exit(status);
	} else {
		exit(status);
	}
}

static int64_t _nnow(void)
{
	struct timespec t;

#ifdef PT_LINUX

	int err;

	err = clock_gettime(CLOCK_MONOTONIC, &t);
	if (err != 0) {
		perror("failed to get time");
		exit(1);
	}


#elif defined(PT_DARWIN)

	// I'm just lazy: http://stackoverflow.com/a/5167506

	static double orwl_timebase = 0.0;
	static uint64_t orwl_timestart = 0;
	double diff;

	if (!orwl_timestart) {
		mach_timebase_info_data_t tb = { 0, 0 };
		mach_timebase_info(&tb);
		orwl_timebase = tb.numer;
		orwl_timebase /= tb.denom;
		orwl_timestart = mach_absolute_time();
	}
	diff = (mach_absolute_time() - orwl_timestart) * orwl_timebase;
	t.tv_sec = diff * (+1.0E-9);
	t.tv_nsec = diff - (t.tv_sec * UINT64_C(1000000000));

#endif

	return (((int64_t)t.tv_sec) * (1000 * 1000 * 1000)) + t.tv_nsec;

}

static int64_t _unow(void)
{
	return _nnow() / 1000;
}

static uint32_t _get_cpu_count(void)
{
	int count;

	count = sysconf(_SC_NPROCESSORS_ONLN);
	if (count > 0) {
		return count;
	}

	return 1;
}

static void _signal_wait(void)
{
#ifdef PT_LINUX

	int err;
	sigset_t mask;
	siginfo_t info;
	struct timespec timeout = {
		.tv_sec = 0,
		.tv_nsec = SLEEP_TIME,
	};

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	err = sigtimedwait(&mask, &info, &timeout);
	if (err < 0 && errno != EAGAIN) {
		perror("sigtimedwait failed");
		exit(1);
	}

#elif defined(PT_DARWIN)

	usleep(SLEEP_TIME);

#endif
}

static void _sigint_handler(int sig)
{
	uint32_t i;

	for (i = 0; i < _jobsc; i++) {
		struct job *j = _jobs + i;
		if (j->pid != -1) {
			killpg(j->pid, SIGKILL);
		}
	}

	signal(SIGINT, SIG_DFL);
	raise(sig);
}

static void _setup_signals(void)
{
	if (!_nofork) {
		signal(SIGINT, _sigint_handler);
	}

	if (1) {

#ifdef PT_LINUX

	int err;
	sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	err = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (err < 0) {
		perror("failed to change signal mask");
		exit(1);
	}

#endif

	}
}

static void _filter_tests(struct tests *ts, const char *filter_)
{
	char *f;
	char *filt;
	uint32_t i;
	char filter[strlen(filter_) + 1];

	strcpy(filter, filter_);
	filt = filter;

	while (1) {
		int neg;

		f = strtok(filt, ", ");
		filt = NULL;
		if (f == NULL) {
			break;
		}

		neg = *f == '-';
		if (neg) {
			f++;
		}

		for (i = 0; i < ts->c; i++) {
			struct test *t = ts->all + i;
			int matches = strstr(t->name, f) == t->name;

			if (neg) {
				if (matches) {
					t->flags.filtered_run = 0;
					t->flags.run = 0;
				}
			} else {
				if (matches) {
					t->flags.filtered_run = 1;
				} else {
					t->flags.run = 0;
				}
			}
		}
	}
}

static uint32_t _parse_uint32(const char *opt, const char *from, uint32_t *to)
{
	char *endptr;
	uint32_t res = strtoul(from, &endptr, 10);

	if (res == 0 || *endptr != '\0') {
		fprintf(stderr, "Invalid value for %s: %s\n", opt, from);
		return 1;
	}

	*to = res;
	return 0;
}

static void _print_opt(const char *s, const char *arg, const char *desc)
{
	printf(INDENT "-%s, --%s\n", s, arg);
	printf(INDENT INDENT "%s\n", desc);
}

static void _print_usage(char **argv)
{
	printf("Usage: %s [OPTION]...\n", argv[0]);
	printf("\n");
	_print_opt("b", "bench", "run benchmarks");
	_print_opt("d 1", "bench-dur=1", "maximum time to run each benchmark for, in seconds (default=1)");
	_print_opt("e", "exit-fast", "after a test has finished, exit without calling any atexit() or on_exit() functions");
	_print_opt("f FILTER", "filter=FILTER,...", "only run tests prefixed with FILTER, may be given multiple times");
	_print_opt("h", "help", "print this messave");
	_print_opt("j #CPU+1", "jobs=#CPU+1", "number of tests to run in parallel");
	_print_opt("n", "nocapture", "don't capture stdout/stderr");
	_print_opt("p " PTSTR(PORT), "port=" PTSTR(PORT), "port number to start handing out ports at");
	_print_opt("s", "nofork", "run every test in a single process without isolation, buffering, or anything else");
	_print_opt("t", "timeout", "set the global timeout for tests, in seconds");
	_print_opt("v", "verbose", "be more verbose with the test summary; pass multiple times to increase verbosity");

	exit(2);
}

static void _set_opt(char **argv, struct tests *ts, const char c)
{
	switch (c) {
		case 0:
			break;

		case 'b': {
			uint32_t i;

			_bench = 1;

			for (i = 0; i < ts->c; i++) {
				struct test *t = ts->all + i;
				t->flags.filtered_run |= t->p->bench;
			}

			break;
		}

		case 'd':
			if (_parse_uint32("bench-dur", optarg, &_bench_duration)) {
				_print_usage(argv);
			}
			break;

		case 'e':
			_exit_fast = 1;
			break;

		case 'f':
			_filter_tests(ts, optarg);
			break;

		case 'j':
			if (_parse_uint32("jobs", optarg, &_max_jobs)) {
				_print_usage(argv);
			}
			break;

		case 'n':
			_nocapture = 1;
			break;

		case 'p':
			if (_parse_uint32("port", optarg, &_port) ||
				_port == 0 ||
				_port > UINT16_MAX) {

				_print_usage(argv);
			}
			break;

		case 't':
			if (_parse_uint32("timeout", optarg, &_timeout)) {
				_print_usage(argv);
			}
			break;

		case 'v':
			if (optarg == NULL) {
				_verbose++;
			} else {
				_verbose += strlen(optarg) + 1;
			}
			break;

		case 's':
			_nofork = 1;
			break;

		case 'h':
		default:
			_print_usage(argv);
	}
}

static void _setenvopt(
	char **argv,
	struct tests *ts,
	const char *name,
	const char c)
{
	char *v = getenv(name);
	if (v == NULL) {
		return;
	}

	optarg = v;
	_set_opt(argv, ts, c);
	optarg = NULL;
}

static void _set_opts(struct tests *ts, int argc, char **argv)
{
	struct option lopts[] = {
		{ "bench", no_argument, &_bench, 'b' },
		{ "bench-dur", required_argument, NULL, 'd' },
		{ "exit-fast", no_argument, &_exit_fast, 'e' },
		{ "filter", required_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "jobs", required_argument, NULL, 'j' },
		{ "nocapture", no_argument, &_nocapture, 'n' },
		{ "nofork", no_argument, &_nofork, 's' },
		{ "port", required_argument, NULL, 'p' },
		{ "timeout", required_argument, NULL, 't' },
		{ "verbose", optional_argument, &_verbose, 'v' },
		{ NULL, 0, NULL, 0 },
	};

	_bench_duration = 1;
	_max_jobs = _get_cpu_count() + 1;
	_port = PORT;
	_timeout = 5;

	_setenvopt(argv, ts, "PTEXITFAST", 'e');
	_setenvopt(argv, ts, "PTFILTER", 'f');
	_setenvopt(argv, ts, "PTJOBS", 'j');
	_setenvopt(argv, ts, "PTNOCAPTURE", 'n');
	_setenvopt(argv, ts, "PTNOFORK", 's');
	_setenvopt(argv, ts, "PTPORT", 'p');
	_setenvopt(argv, ts, "PTTIMEOUT", 't');
	_setenvopt(argv, ts, "PTVERBOSE", 'v');
	_setenvopt(argv, ts, "PTBENCH", 'b');
	_setenvopt(argv, ts, "PTBENCHDUR", 'd');

	while (1) {
		char c = getopt_long(argc, argv, "bd:ef:hj:np:st:v::", lopts, NULL);
		if (c == -1) {
			break;
		}

		_set_opt(argv, ts, c);
	}
}

static void _add_test(struct tests *ts, struct paratec *p)
{
	int64_t i;
	char idx[34];

	struct test t = {
		.p = p,
		.flags = {
			.run = !p->bench,
			.passed = 0,
		},
	};

	if (p->range_low == 0 && p->range_high == 0) {
		p->range_high = 1;
	} else {
		t.flags.is_ranged = 1;
	}

	for (i = p->range_low; i < p->range_high; i++) {
		if (ts->c == ts->allocated) {
			ts->allocated = MAX(ts->allocated, 1) * 2;
			ts->all = realloc(ts->all, sizeof(*ts->all) * ts->allocated);
			if (ts->all == NULL) {
				fprintf(stderr, "failed to allocate array for tests\n");
				exit(1);
			}
		}

		if (t.flags.is_ranged) {
			snprintf(idx, sizeof(idx), ":%" PRId64, i);
		} else {
			idx[0] = '\0';
		}

		t.i = i;
		t.titem = p->vec == NULL ?
			NULL :
			((char*)p->vec) + (i * p->vecisize);
		snprintf(t.name, sizeof(t.name), "%s%s", p->name, idx);

		ts->all[ts->c++] = t;
	}
}

static void _flush_pipe(int fd, struct buff *b)
{
	ssize_t err;
	ssize_t cap;

	do {
		if ((b->len + 1) >= b->alloc) {
			b->alloc = sizeof(*b->str) * (MAX(b->alloc, 16) * 2);
			b->str = realloc(b->str, b->alloc);
			if (b->str == NULL) {
				fprintf(stderr, "failed to allocate buffer for test output\n");
				exit(1);
			}
		}

		cap = (b->alloc - b->len) - 1;
		err = read(fd, b->str + b->len, cap);
		if (err < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				perror("failed to read test output");
				exit(1);
			}
		} else {
			b->len += err;
			b->str[b->len] = '\0';
		}
	} while (err == cap);
}

static void _flush_pipes(struct tests *ts, struct job *jobs, uint32_t jobsc)
{
	uint32_t i;

	if (_nocapture) {
		return;
	}

	for (i = 0; i < jobsc; i++) {
		struct job *j = jobs + i;

		if (j->pid == -1) {
			continue;
		}

		_flush_pipe(j->stdout, &ts->all[j->i].stdout);
		_flush_pipe(j->stderr, &ts->all[j->i].stdout);
	}
}

static void _check_timeouts(struct job *jobs, uint32_t jobsc)
{
	int err;
	uint32_t i;
	int64_t now = _unow();

	for (i = 0; i < jobsc; i++) {
		struct job *j = jobs + i;
		if (j->pid != -1 && j->end_at < now) {
			j->timed_out = 1;
			err = killpg(j->pid, SIGKILL);
			if (err < 0 && errno != ESRCH) {
				perror("failed to kill child test");
				exit(1);
			}
		}
	}
}

static void _pipe(int p[2])
{
	int err = pipe(p);
	if (err < 0) {
		perror("failed to create pipes");
		exit(1);
	}

	err = fcntl(p[0], F_SETFL, O_NONBLOCK);
	if (err < 0) {
		perror("failed to make test pipe nonblocking");
		exit(1);
	}
}

static void _dup2(int std, int fd)
{
	int err = dup2(fd, std);
	if (err < 0) {
		perror("failed to dup2");
		exit(1);
	}
}

static void _run_test_case(struct test *t)
{
	if (t->p->setup != NULL) {
		t->p->setup();
	}

	t->p->fn(t->i, 0, t->titem);

	if (t->p->teardown != NULL) {
		t->p->teardown();
	}
}

static uint32_t _nearest_pow_10(uint32_t n)
{
	uint32_t i;
	uint32_t res = 1;
	uint32_t tens = 0;

	while (n >= 10) {
		n = n / 10;
		tens++;
	}

	for (i = 0; i < tens; i++) {
		res *= 10;
	}

	return res;
}

static uint32_t _round_up(uint32_t n)
{
	static const int is[] = { 1, 2, 3, 5 };

	uint32_t i;
	uint32_t base = _nearest_pow_10(n);

	for (i = 0; i < N_ELEMENTS(is); i++) {
		if (n <= (is[i] * base)) {
			return is[i] * base;
		}
	}

	return 10 * base;
}

// This was pretty much lifted from Golang's benchmarking
static void _run_bench(struct test *t, struct job *j)
{
	int64_t start;
	uint32_t N = 1;
	int64_t lastN = 0;
	int64_t ns_op = 0;
	int64_t duration = 0;
	const int64_t max_duration = _bench_duration * 1000 * 1000 * 1000;

	while (N < MAX_BENCH_ITERS && duration < max_duration) {
		lastN = N;

		if (t->p->setup != NULL) {
			t->p->setup();
		}

		start = _nnow();

		t->p->fn(t->i, N, t->titem);

		duration = _nnow() - start;
		ns_op = duration / N;

		if (ns_op == 0) {
			N = MAX_BENCH_ITERS;
		} else {
			N = max_duration / ns_op;
		}

		N = MAX(MIN(N + N / 5, 100 * lastN), lastN + 1);
		N = _round_up(N);


		if (t->p->teardown != NULL) {
			t->p->teardown();
		}
	}

	j->bench.iters = lastN;
	j->bench.ns_op = ns_op;
}

static void _run_test(struct test *t, struct job *j)
{
	_tjob = j;
	strncpy(_tjob->fn_name, t->p->fn_name, sizeof(_tjob->fn_name));
	strncpy(_tjob->test_name, t->name, sizeof(_tjob->test_name));
	strncpy(_tjob->last_fn_line, "test start", sizeof(_tjob->last_fn_line));

	if (t->p->bench) {
		_run_bench(t, j);
	} else {
		_run_test_case(t);
	}
}

static void _cleanup_job(struct tests *ts, struct job *j, struct test *t)
{
	if (!_nocapture) {
		close(j->stdout);
		close(j->stderr);
	}

	t->duration = RUNTIME(j->start);
	t->bench = j->bench;

	strncpy(t->fail_msg, j->fail_msg, sizeof(t->fail_msg));
	if (*j->last_line != '\0') {
		snprintf(t->last_line, sizeof(t->last_line), "%s (%s)",
			j->last_fn_line,
			j->last_line);
	} else {
		strncpy(t->last_line, j->last_fn_line, sizeof(t->last_line));
	}

	if (t->p->cleanup != NULL) {
		_tjob = j;
		t->p->cleanup();
	}

	if (t->flags.is_ranged && *j->iter_name != '\0') {
		snprintf(t->name, sizeof(t->name), "%s:%" PRId64 ":%s",
			t->p->name,
			t->i,
			j->iter_name);
	}

	if (j->skipped) {
		ts->enabled--;
		t->flags.run = 0;
	}

	j->pid = -1;
	j->stdout = -1;
	j->stderr = -1;
	j->skipped = 0;
	j->timed_out = 0;
	j->start = INT64_MIN;
	j->end_at = INT64_MIN;
	*j->iter_name = '\0';
	*j->test_name = '\0';
	*j->last_line = '\0';
	*j->last_fn_line = '\0';
	*j->fail_msg = '\0';
}

static void _run_fork_test(struct test *t, struct job *j)
{
	int err;
	pid_t pid;
	uint32_t i;
	int pipes[3][2];

	if (!_nocapture) {
		for (i = 0; i < 3; i++) {
			_pipe(pipes[i]);
		}
	}

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "failed to fork for test %s: %s\n",
			t->p->name,
			strerror(errno));
		exit(1);
	}

	if (pid == 0) {
		if (!_nocapture) {
			for (i = 0; i < 3; i++) {
				close(pipes[i][0]);
				_dup2(i, pipes[i][1]);
			}
		}

		signal(SIGINT, SIG_DFL);
		err = setpgid(0, 0);
		if (err < 0) {
			perror("could not setpgid");
			exit(1);
		}

		_run_test(t, j);

		_exit_test(0);
	} else {
		if (!_nocapture) {
			for (i = 0; i < 3; i++) {
				close(pipes[i][1]);
			}

			close(pipes[STDIN_FILENO][0]);
			j->stdout = pipes[STDOUT_FILENO][0];
			j->stderr = pipes[STDERR_FILENO][0];
		}

		j->pid = pid;
		j->start = _unow();
		j->end_at = j->start + ((t->p->timeout ?: _timeout) * 1000 * 1000);
	}
}

static void _run_next_fork_test(struct tests *ts, struct job *j)
{
	while (ts->next_test < ts->c) {
		uint32_t i = ts->next_test++;
		struct test *t = ts->all + i;

		if (t->flags.run) {
			_run_fork_test(t, j);
			j->i = i;
			break;
		}
	}
}

static void _run_fork_tests(struct tests *ts)
{
	int err;
	pid_t pid;
	uint32_t i;
	int status;
	struct job *j;
	struct job *jobsmm;
	struct job jobs[_max_jobs];
	uint32_t finished = 0;

	assert(N_ELEMENTS(jobs) > 0);

	if (ts->c == 0) {
		return;
	}

	if (!_nocapture) {
		err = setenv("LIBC_FATAL_STDERR_", "1", 1);
		if (err < 0) {
			perror("failed to set LIBC_FATAL_STDERR_");
			exit(1);
		}
	}

	jobsmm = mmap(
		NULL, sizeof(jobs),
		PROT_READ | PROT_WRITE,
		MAP_ANON | MAP_SHARED, -1, 0);
	if (jobsmm == MAP_FAILED) {
		perror("could not map shared testing memory");
		exit(1);
	}

	_jobs = jobsmm;
	_jobsc = N_ELEMENTS(jobs);
	for (i = 0; i < N_ELEMENTS(jobs); i++) {
		j = jobsmm + i;

		j->id = i;
		j->pid = -1;
	}

	for (i = 0; i < N_ELEMENTS(jobs) && i < ts->c; i++) {
		j = jobsmm + i;
		_run_next_fork_test(ts, j);
	}

	while (finished < ts->enabled) {
		char summary_char;
		struct test *t = NULL;

		_signal_wait();

		while (finished < ts->enabled) {
			pid = waitpid(-1, &status, WNOHANG);
			if (pid == -1) {
				perror("wait() failed");
				exit(1);
			}

			_flush_pipes(ts, jobsmm, N_ELEMENTS(jobs));

			if (pid == 0) {
				break;
			}

			for (i = 0; i < N_ELEMENTS(jobs); i++) {
				j = jobsmm + i;
				if (j->pid == pid) {
					t = ts->all + j->i;
					break;
				}
			}

			if (t == NULL) {
				fprintf(stderr,
					"child exited (pid=%d), but test case can't be found?\n",
					pid);
				exit(1);
			}

			if (WIFEXITED(status)) {
				t->exit_status = WEXITSTATUS(status);
				t->flags.passed = t->exit_status == t->p->exit_status;
			} else if (WIFSIGNALED(status)) {
				t->signal_num = WTERMSIG(status);
				t->flags.passed = t->signal_num == t->p->signal_num;
			} else {
				continue;
			}

			if (j->skipped) {
				summary_char = 'S';
			} else if (t->flags.passed || (!t->flags.passed && t->p->expect_fail)) {
				t->flags.passed = 1;
				ts->passes++;
				summary_char = '.';
			} else if (t->signal_num > 0) {
				ts->errors++;
				t->flags.timed_out = j->timed_out;
				summary_char = 'E';
			} else {
				ts->failures++;
				summary_char = 'F';
			}

			if (!_nocapture) {
				printf("%c", summary_char);
				fflush(stdout);
			}

			if (!j->skipped) {
				finished++;
			}

			_cleanup_job(ts, j, t);
			_run_next_fork_test(ts, j);
		}

		_check_timeouts(jobsmm, N_ELEMENTS(jobs));
	}

	printf("\n");
}

static void _run_nofork_test(struct tests *ts, struct test *t, struct job *j)
{
	if (setjmp(_tfail) == 0) {
		j->start = _unow();
		_run_test(t, j);

		ts->passes++;
		t->flags.passed = 1;
	} else {
		if (!j->skipped) {
			if (t->p->expect_fail) {
				ts->passes++;
				t->flags.passed = 1;
			} else {
				ts->failures++;
				t->exit_status = FAIL_EXIT_STATUS;
				t->flags.passed = 0;
			}
		}
	}

	_cleanup_job(ts, j, t);
}

static void _run_nofork_tests(struct tests *ts)
{
	uint32_t i;
	struct job j;

	memset(&j, 0, sizeof(j));
	j.pid = j.stdout = j.stderr = -1;

	for (i = 0; i < ts->c; i++) {
		struct test *t = ts->all + i;
		size_t len = MAX(strlen(t->name), 70);
		char underline[9 + len + 1];

		if (!t->flags.run) {
			continue;
		}

		memset(underline, '=', sizeof(underline));
		underline[sizeof(underline) - 1] = '\0';
		printf("Running: %s\n", t->name);
		printf("%s\n\n", underline);

		_run_nofork_test(ts, t, &j);

		printf("\n%s\n", underline);
	}
}

static void _run_tests(struct tests *ts)
{
	if (_nofork) {
		_run_nofork_tests(ts);
	} else {
		_run_fork_tests(ts);
	}
}

static void _dump_line(const char *line, const size_t len)
{
	fwrite(STDPREFIX, strlen(STDPREFIX), 1, stdout);
	fwrite(line, len, 1, stdout);
	fwrite("\n", 1, 1, stdout);
}

static int _clear_buff(int dump, const char *which, struct buff *b)
{
	dump = dump && !_nofork && b->len > 0;

	if (dump) {
		int first = 1;
		char *start = b->str;
		size_t len = b->len;

		printf(INDENT INDENT "%s\n", which);

		while (1) {
			char *nl = memmem(start, len, "\n", 1);
			if (nl == NULL) {
				if (first) {
					_dump_line(start, len);
				}
				break;
			}

			first = 0;
			*nl = '\0';
			_dump_line(start, nl - start);
			len -= (nl - start) + 1;
			start = nl + 1;
		}
	}

	free(b->str);
	b->str = NULL;

	return dump;
}

int main(int argc, char **argv)
{
	int64_t start;
	uint32_t i;
	struct tests ts;
	struct paratec **sp;

#ifdef PT_LINUX

	/**
	 * There's something weird going on with GCC/LD on Linux: when not using a
	 * pointer, anything larger than a few words causes the section to be
	 * misaligned, and you can't access anything.
	 */
	extern struct paratec *__start_paratec;
	extern struct paratec *__stop_paratec;

#elif defined(PT_DARWIN)

	extern struct paratec *__start_paratec
		__asm("section$start$__DATA$" PT_SECTION_NAME);
	extern struct paratec *__stop_paratec
		__asm("section$end$__DATA$" PT_SECTION_NAME);

#endif

	setlocale(LC_NUMERIC, "");

	memset(&ts, 0, sizeof(ts));
	_pthself = pthread_self();
	_setup_signals();

	sp = &__start_paratec;
	while (sp < &__stop_paratec) {
		struct paratec *p = *sp;
		_add_test(&ts, p);
		sp++;
	}

	_set_opts(&ts, argc, argv);

	for (i = 0; i < ts.c; i++) {
		struct test *t = ts.all + i;
		if (t->flags.run || t->flags.filtered_run) {
			t->flags.run = 1;
			ts.enabled++;
		}

		if (t->p->bench) {
			t->p->timeout = t->p->timeout ?: _bench_duration * 2;
		}
	}

	start = _unow();
	_run_tests(&ts);

	printf("%d%%: %" PRIu32 " tests, %" PRIu32 " errors, %" PRIu32 " failures, %" PRIu32 " skipped. Ran in %fs\n",
		ts.enabled == 0 ?
			100 :
			(int)((((double)ts.passes) / ts.enabled) * 100),
		ts.enabled,
		ts.errors,
		ts.failures,
		ts.c - ts.enabled,
		RUNTIME(start));

	for (i = 0; i < ts.c; i++) {
		int dump = 1;
		int dumped = 0;
		struct test *t = ts.all + i;

		if (!t->flags.run) {
			dump = 0;

			if (_verbose >= 2) {
				dump = 1;
				printf(INDENT " SKIP : %s \n",
					t->name);
			}
		} else if (t->flags.passed) {
			if (t->p->bench) {
				dump = 1;
				printf(INDENT "BENCH : %s (%'" PRIu64 " @ %'" PRIu64 " ns/op)\n",
					t->name,
					t->bench.iters,
					t->bench.ns_op);
			} else {
				if (_verbose) {
					printf(INDENT " PASS : %s (%fs)\n",
						t->name,
						t->duration);
				}
				dump &= _verbose >= 3;
			}
		} else if (t->exit_status == FAIL_EXIT_STATUS) {
			printf(INDENT " FAIL : %s (%fs) : %s : %s\n",
				t->name,
				t->duration,
				t->last_line,
				t->fail_msg);
		} else if (t->exit_status != 0) {
			printf(INDENT " FAIL : %s (%fs) : after %s : exit code=%d\n",
				t->name,
				t->duration,
				t->last_line,
				t->exit_status);
		} else if (t->flags.timed_out) {
			printf(INDENT "ERROR : %s (%fs) : after %s : timed out\n",
				t->name,
				t->duration,
				t->last_line);
		} else if (t->signal_num != 0) {
			printf(INDENT "ERROR : %s (%fs) : after %s : received signal(%d) `%s`\n",
				t->name,
				t->duration,
				t->last_line,
				t->signal_num,
				strsignal(t->signal_num));
		}

		dumped = _clear_buff(dump, "stdout", &t->stdout);
		dumped |= _clear_buff(dump, "stderr", &t->stderr);

		if (dumped) {
			printf("\n");
		}
	}

	free(ts.all);
	ts.all = NULL;

	return ts.passes != ts.enabled;
}

void pt_skip(void)
{
	_tjob->skipped = 1;
	_exit_test(0);
}

uint16_t pt_get_port(uint8_t i)
{
	return _port + _tjob->id + (i * _max_jobs);
}

const char* pt_get_name()
{
	return _tjob->test_name;
}

void pt_set_iter_name(
	const char *format,
	...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(_tjob->iter_name, sizeof(_tjob->iter_name), format, args);
	va_end(args);
}

void _pt_fail(
	const char *format,
	...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(_tjob->fail_msg, sizeof(_tjob->fail_msg), format, args);
	va_end(args);

	fflush(stdout);
	fflush(stderr);

	if (_nofork) {
		if (!pthread_equal(_pthself, pthread_self())) {
			printf(
				"************************************************************\n"
				"*                          ERROR                           *\n"
				"*                                                          *\n"
				"*  Whoa there! You can't make assertions from any thread   *\n"
				"*  but the testing thread when running in single-process   *\n"
				"*  mode. The faulty assertion follows.                     *\n"
				"*                                                          *\n"
				"************************************************************\n"
				"\n"
				"%s : %s\n",
				_tjob->last_line,
				_tjob->fail_msg);

			fflush(stdout);
			abort();
		}
	}

	_exit_test(FAIL_EXIT_STATUS);
}

void _pt_mark(
	const char *file,
	const char *func,
	const size_t line)
{
	if (strcmp(_tjob->fn_name, func) == 0) {
		*_tjob->last_line = '\0';
		snprintf(_tjob->last_fn_line, sizeof(_tjob->last_fn_line), "%s:%zu", file, line);
	} else {
		snprintf(_tjob->last_line, sizeof(_tjob->last_line), "%s:%zu", file, line);
	}
}
