#include <signal.h>
#include <stdbool.h>
#include <stdio.h>

#include "dev_input_set.h"
#include "inotify_thread.h"

int main(int argc, char **argv)
{
	int rc = 1; /* Error */

	dev_input_set_init();

	/*
	 * Mask all signals before starting other threads.
	 * Child threads inherit main thread's signal mask.
	 */
	sigset_t sigset;
	if (0 != sigfillset(&sigset)) {
		fprintf(stderr, "error: failed to initialize signal set: %m\n");
		goto out; /* Error */
	}
	if (0 != sigprocmask(SIG_SETMASK, &sigset, NULL)) {
		fprintf(stderr, "error: failed to set signal mask: %m\n");
		goto out; /* Error */
	}

	/*
	 * Watch for /dev/input/event* files and spawn handlers as they appear.
	 * Also spawn handlers for those that are already present on startup.
	 */
	if (0 != spawn_inotify_thread()) {
		goto out; /* Error */
	}

	/* Wait for signal to exit */
	int sig;
	do {
		if (0 != sigwait(&sigset, &sig)) {
			fprintf(stderr, "error: failed to wait for signals: %m\n");
			goto out; /* Error */
		}
	} while (sig != SIGTERM && sig != SIGINT);

	rc = 0;

out:
	return rc;
}
