/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "fork.hpp"
#include "util_test.hpp"

namespace pt
{

TEST(forkForceTerminate)
{
	Fork f;
	int err;
	int status;

	bool parent = f.fork(false, true);
	if (!parent) {
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		while (true) {
			usleep(10000);
		}
	}

	err = f.terminate(&status);
	pt_eq(err, f.pid());
	pt(WIFSIGNALED(status));
	pt_eq(WTERMSIG(status), SIGKILL);
}
}
