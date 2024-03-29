#include <signal.h>
#include <stdbool.h>
#include <stdio.h>

#include "dev_input_set.h"
#include "inotify_thread.h"
#include "stats_thread.h"
#include "stats_flush_thread.h"
#include "journal.h"

int main(int argc, char **argv)
{
	int rc = 1; /* Error */

	dev_input_set_init();

	if (0 != journal_init()) {
		goto out; /* Error */
	}

	if (0 != status_flush_thread_init()) {
		goto out; /* Error */
	}

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

	/* All events will flow into this thread. */
	if (0 != spawn_stats_thread()) {
		goto out; /* Error */
	}

	/* This thread periodically inserts flush events. */
	if (0 != spawn_stats_flush_thread()) {
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
	journal_fini();

	return rc;
}
