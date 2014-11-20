/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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

#define XSTR(s) #s
#define STR(s) XSTR(s)

#define SLEEP_TIME (10 * 1000)
#define PORT 43120
#define FAIL_EXIT_STATUS 255
#define INDENT "    "
#define STDPREFIX INDENT INDENT " | "
#define LINE_SIZE 2048
#define FAILMSG_SIZE 8192

#define RUNTIME(start) ((_now() - start) / ((double)(1000 * 1000)))
#define MAX(a, b) (a > b ? a : b)
#define N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

struct job {
	uint32_t id;
	pid_t pid;
	int stdout;
	int stderr;
	uint32_t i; // Index of the test in tests.all
	int64_t start;
	int64_t end_at;
	int timed_out;
	char test_name[LINE_SIZE];
	char last_line[LINE_SIZE];
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
	struct buff stdout;
	struct buff stderr;
	char last_line[LINE_SIZE];
	char fail_msg[FAILMSG_SIZE];
	struct paratec *p;
	struct {
		int run:1;
		int filtered_run:1;
		int passed:1;
		int timed_out:1;
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

static int64_t _now()
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

	return (((int64_t)t.tv_sec) * 1000000) + (t.tv_nsec / 1000);
}

static uint32_t _get_cpu_count()
{
	int count;

	count = sysconf(_SC_NPROCESSORS_ONLN);
	if (count > 0) {
		return count;
	}

	return 1;
}

static void _signal_wait()
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

static void _setup_signals()
{
	if (!_nofork) {
		signal(SIGINT, _sigint_handler);
	}

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

static void _print_opt(char *s, char *arg, char *desc)
{
	printf(INDENT "-%s, --%s\n", s, arg);
	printf(INDENT INDENT "%s\n", desc);
}

static void _print_usage(char **argv)
{
	printf("Usage: %s [OPTION]...\n", argv[0]);
	printf("\n");
	_print_opt("f FILTER", "filter=FILTER,...", "only run tests prefixed with FILTER, may be given multiple times");
	_print_opt("h", "help", "print this messave");
	_print_opt("j #CPU+1", "jobs=#CPU+1", "number of tests to run in parallel");
	_print_opt("n", "nocapture", "don't capture stdout/stderr");
	_print_opt("p " STR(PORT), "port=" STR(PORT), "port number to start handing out ports at");
	_print_opt("s", "nofork", "run every test in a single process without isolation, buffering, or anything else");
	_print_opt("t", "timeout", "set the global timeout for tests, in seconds");
	_print_opt("v", "verbose", "print information about tests that succeed");

	exit(2);
}

static void _set_opt(char **argv, struct tests *ts, const char c)
{
	switch (c) {
		case 0:
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
			_verbose = 1;
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
		{ "filter", required_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "jobs", required_argument, NULL, 'j' },
		{ "nocapture", no_argument, &_nocapture, 'n' },
		{ "nofork", no_argument, &_nofork, 's' },
		{ "port", required_argument, NULL, 'p' },
		{ "timeout", required_argument, NULL, 't' },
		{ "verbose", no_argument, &_verbose, 'v' },
		{ NULL, 0, NULL, 0 },
	};

	_max_jobs = _get_cpu_count() + 1;
	_port = PORT;
	_timeout = 5;

	_setenvopt(argv, ts, "PTFILTER", 'f');
	_setenvopt(argv, ts, "PTJOBS", 'j');
	_setenvopt(argv, ts, "PTNOCAPTURE", 'n');
	_setenvopt(argv, ts, "PTNOFORK", 's');
	_setenvopt(argv, ts, "PTPORT", 'p');
	_setenvopt(argv, ts, "PTTIMEOUT", 't');
	_setenvopt(argv, ts, "PTVERBOSE", 'v');

	while (1) {
		char c = getopt_long(argc, argv, "f:hj:np:st:v", lopts, NULL);
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
	int has_range = 0;

	struct test t = {
		.p = p,
		.flags = {
			.run = 1,
			.passed = 0,
		},
	};

	if (p->range_low == 0 && p->range_high == 0) {
		p->range_high = 1;
	} else {
		has_range = 1;
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

		if (has_range) {
			snprintf(idx, sizeof(idx), ":%" PRId64, i);
		} else {
			idx[0] = '\0';
		}

		t.i = i;
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
	int64_t now = _now();

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

static void _run_test(struct test *t, struct job *j)
{
	_tjob = j;
	strncpy(_tjob->test_name, t->name, sizeof(_tjob->test_name));
	strncpy(_tjob->last_line, "test start", sizeof(_tjob->last_line));

	if (t->p->setup != NULL) {
		t->p->setup();
	}

	t->p->fn(t->i);

	if (t->p->teardown != NULL) {
		t->p->teardown();
	}
}

static void _cleanup_job(struct job *j, struct test *t)
{
	if (!_nocapture) {
		close(j->stdout);
		close(j->stderr);
	}

	t->duration = RUNTIME(j->start);
	strncpy(t->last_line, j->last_line, sizeof(t->last_line));
	strncpy(t->fail_msg, j->fail_msg, sizeof(t->fail_msg));

	j->pid = -1;
	j->stdout = -1;
	j->stderr = -1;
	j->timed_out = 0;
	j->start = INT64_MIN;
	j->end_at = INT64_MIN;
	*j->test_name = '\0';
	*j->last_line = '\0';
	*j->fail_msg = '\0';

	if (t->p->cleanup != NULL) {
		t->p->cleanup(t->name);
	}
}

static void _run_fork_test(struct test *t, struct job *j)
{
	int err;
	pid_t pid;
	int pstdin[2];
	int pstdout[2];
	int pstderr[2];

	if (!_nocapture) {
		_pipe(pstdin);
		_pipe(pstdout);
		_pipe(pstderr);
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
			_dup2(STDIN_FILENO, pstdin[1]);
			_dup2(STDOUT_FILENO, pstdout[1]);
			_dup2(STDERR_FILENO, pstderr[1]);
		}

		signal(SIGINT, SIG_DFL);
		err = setpgid(0, 0);
		if (err < 0) {
			perror("could not setpgid");
			exit(1);
		}

		_run_test(t, j);

		exit(0);
	} else {
		if (!_nocapture) {
			close(pstdin[0]);
			j->stdout = pstdout[0];
			j->stderr = pstderr[0];
		}

		j->pid = pid;
		j->start = _now();
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
	pid_t pid;
	uint32_t i;
	int status;
	struct job *j;
	struct job *jobsmm;
	struct job jobs[_max_jobs];
	uint32_t finished = 0;

	if (ts->c == 0) {
		return;
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
		struct test *t = NULL;

		_signal_wait();

		pid = waitpid(-1, &status, WNOHANG);
		if (pid == -1) {
			perror("wait() failed");
			exit(1);
		}

		if (pid > 0) {
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

			_flush_pipes(ts, jobsmm, N_ELEMENTS(jobs));

			if (t->flags.passed || (!t->flags.passed && t->p->expect_fail)) {
				t->flags.passed = 1;
				ts->passes++;
				printf(".");
			} else if (t->signal_num > 0) {
				ts->errors++;
				t->flags.timed_out = j->timed_out;
				printf("E");
			} else {
				ts->failures++;
				printf("F");
			}

			fflush(stdout);

			_cleanup_job(j, t);
			_run_next_fork_test(ts, j);
			finished++;
		}

		_flush_pipes(ts, jobsmm, N_ELEMENTS(jobs));
		_check_timeouts(jobsmm, N_ELEMENTS(jobs));
	}

	printf("\n");
}

static void _run_nofork_test(struct tests *ts, struct test *t, struct job *j)
{
	if (setjmp(_tfail) == 0) {
		_run_test(t, j);

		ts->passes++;
		t->flags.passed = 1;
	} else {
		if (t->p->expect_fail) {
			ts->passes++;
			t->flags.passed = 1;
		} else {
			ts->failures++;
			t->exit_status = FAIL_EXIT_STATUS;
			t->flags.passed = 0;
		}
	}

	_cleanup_job(j, t);
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
		printf(INDENT INDENT "%s\n", which);

		int first = 1;
		char *start = b->str;
		size_t len = b->len;

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
			len -= nl - start;
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

	memset(&ts, 0, sizeof(ts));

	_setup_signals();

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

	struct paratec **sp = &__start_paratec;

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
	}

	start = _now();
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
			if (_verbose) {
				printf(INDENT " SKIP : %s \n",
					t->name);
			}
		} else if (t->flags.passed) {
			dump = _verbose;
			if (dump) {
				printf(INDENT " PASS : %s (%fs)\n",
					t->name,
					t->duration);
			}
		} else if (t->exit_status == FAIL_EXIT_STATUS) {
			printf(INDENT " FAIL : %s (%fs) : at %s : %s\n",
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

uint16_t pt_get_port(uint8_t i)
{
	return _port + _tjob->id + (i * _max_jobs);
}

const char* pt_get_name()
{
	return _tjob->test_name;
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
		longjmp(_tfail, 1);
	} else {
		exit(FAIL_EXIT_STATUS);
	}
}

void _pt_mark(
	const char *file,
	const size_t line)
{
	snprintf(_tjob->last_line, sizeof(_tjob->last_line), "%s:%zu", file, line);
}
