#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <unistd.h>
#include <unistd.h>
#include <unistd.h>
#include <unistd.h>

#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>

#include "dev_input_set.h"

const char * const dev_input_path = "/dev/input";

struct dev_input_event_thread_data {
	char *path;
	struct libevdev *dev;
};

void dev_input_event_thread_data_free(struct dev_input_event_thread_data *h)
{
	if (!h) {
		return;
	}

	/*
	 * libevdev doesn't manage fd's, it only uses them.
	 * So up to us to close it.
	 */
	if (h->dev) {
		int fd = libevdev_get_fd(h->dev);
		if (fd >= 0) {
			close(fd);
		}
	}

	libevdev_free(h->dev);

	/*
	 * Device was added to set before thread was spawned to prevent connecting twice.
	 * We're disconnected now, so we can remove it again, in case it re-appears.
	 * Bug: this is racy - if it reappears too fast, we will miss it. ¯\_(ツ)_/¯
	 */
	dev_input_set_remove(h->path);

	free(h->path);
	free(h);
}

static void dev_input_event_thread_handle_event(const struct input_event *event)
{
	if (!libevdev_event_is_type(event, EV_KEY)) {
		return;
	}

	/* Not a key down event? */
	if (event->value != 1) {
		return;
	}

	const char *p = libevdev_event_code_get_name(event->type, event->code) + 4;
	if (strlen(p) == 1) {
		printf("debug: key: %c\n", *p);
	} else {
		printf("debug: key: <%s>\n", p);
	}
}

static void *dev_input_event_thread(void *arg)
{
	struct dev_input_event_thread_data *thread = (struct dev_input_event_thread_data *)arg;
	struct input_event event;
	int rc;

	fprintf(stderr, "info: attached to %s (%s)\n", thread->path, libevdev_get_name(thread->dev));

	do {
		rc = libevdev_next_event(
				thread->dev,
				LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING,
				&event);

		if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
			dev_input_event_thread_handle_event(&event);
		}

	} while (rc == LIBEVDEV_READ_STATUS_SYNC
			|| rc == LIBEVDEV_READ_STATUS_SUCCESS
			|| rc == -EAGAIN);

	dev_input_event_thread_data_free(thread);

	return NULL;
}

void spawn_dev_input_event_thread(char *path)
{
	/* Prevent spawning multiple threads for same path */
	if (0 != dev_input_set_add(path)) {
		return; /* Error, or already being handled */
	}

	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "error: failed to open %s\n", path);
		return;
		goto err_1;
	}

	struct dev_input_event_thread_data *thread = calloc(sizeof(*thread), 1);
	if (!thread) {
		fprintf(stderr, "error: %m\n");
		goto err_2;
	}

	thread->path = strdup(path);
	if (!thread->path) {
		fprintf(stderr, "error: %m\n");
		goto err_3;
	}

	if (libevdev_new_from_fd(fd, &thread->dev) < 0) {
		fprintf(stderr, "error: failed to init libevdev dev: %m\n");
		goto err_3;
	}

	/* Not a keyboard? */
	if (!libevdev_has_event_type(thread->dev, EV_KEY)) {
		goto err_3;
	}

	/* Initialize the pthread attribute object */
	pthread_attr_t pthread_attr;
	errno = pthread_attr_init(&pthread_attr);
	if (0 != errno) {
		fprintf(stderr, "error: failed to initialize pthread_attr_t: %m\n");
		goto err_3;
	}

	/* Since no return value is required, create detached threads. */
	errno = pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
	if (0 != errno) {
		fprintf(stderr, "error: failed to set thread detached state: %m\n");
		goto err_4;
	}

	pthread_t tid;
	errno = pthread_create(&tid, &pthread_attr, dev_input_event_thread, thread);
	if (0 != errno) {
		fprintf(stderr, "error: failed to create thread: %m\n");
		goto err_4;
	}

	pthread_attr_destroy(&pthread_attr);

	return; /* Success */

err_4:
	pthread_attr_destroy(&pthread_attr);
err_3:
	dev_input_event_thread_data_free(thread);
err_2:
	close(fd);
err_1:
	return; /* Error */
}

int is_event_filename(char *name)
{
	regex_t regex;
	int regcomp_result;

	regcomp_result = regcomp(&regex, "^event[0-9]+$", REG_EXTENDED);
	if (0 != regcomp_result) {
		char regcomp_errbuf[1024]; // NOLINT(readability-magic-numbers)
		regerror(regcomp_result, &regex, regcomp_errbuf, sizeof(regcomp_errbuf));
		fprintf(stderr, "error: failed to compile regex: %s\n", regcomp_errbuf);
		return -1; /* Error */
	}

	int ret = (0 == regexec(&regex, name, 0, NULL, 0));
	regfree(&regex);
	return ret;
}

static void scan_event_files(void *arg)
{
	DIR *dir;
	struct dirent *ent;

	dir = opendir(dev_input_path);
	if (!dir) {
		fprintf(stderr, "error: failed to open %s: %m\n", dev_input_path);
		goto out_1; /* Error */
	}

	while ((ent = readdir(dir))) {
		switch (is_event_filename(ent->d_name)) {
		case 1: {
			char path[PATH_MAX];
			snprintf(path, sizeof(path), "%s/%s", dev_input_path, ent->d_name);
			spawn_dev_input_event_thread(path);
			break;
		}
		case 0:
			continue; /* Not an event* file */
		default:
			goto out_2; /* Error */
		}
	}

out_2:
	closedir(dir);
out_1:
	return;
}

static void handle_inotify_event(struct inotify_event *i)
{
	if (!(i->mask & IN_CREATE)) {
		return; /* Not a create event */
	}

	if (i->len <= 0) {
		return; /* No filename present */
	}

	if (!is_event_filename(i->name)) {
		return; /* Not an event file */
	}

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", dev_input_path, i->name);
	spawn_dev_input_event_thread(path);
}

static void *inotify_thread(void *arg)
{
	int inotify_fd = inotify_init();
	if (inotify_fd < 0) {
		fprintf(stderr, "error: failed to initialize inotify: %m\n");
		goto err_1;
	}

	if (inotify_add_watch(inotify_fd, dev_input_path, IN_CREATE) < 0) {
		fprintf(stderr, "error: failed to add inotify watch for %s: %m\n", dev_input_path);
		goto err_2;
	}

	/* Now that inotify has been setup, we can scan for pre-existing files in a race free way. */
	scan_event_files(NULL) ;

	while (true) {
		char buf[16 * (sizeof(struct inotify_event) + NAME_MAX + 1)]; // NOLINT(readability-magic-numbers)

		int num_read = read(inotify_fd, buf, sizeof(buf));
		if (num_read == 0) {
			fprintf(stderr, "error: EOF on inotify stream\n");
			goto err_2;
		} else if (num_read < 0) {
			fprintf(stderr, "error: read from inotify stream failed: %m\n");
			goto err_2;
		}

		char *buf_ptr;
		char *buf_end = buf + num_read;
		struct inotify_event *event;
		for (buf_ptr = buf; buf_ptr < buf_end; buf_ptr += sizeof(struct inotify_event) + event->len) {
			event = (struct inotify_event *) buf_ptr;

			/*
			 * Check for partial or truncated events. Should be impossible.
			 * An author of inotifytools.c of inotify-tools agrees.
			 */
			if (buf_ptr + sizeof(struct inotify_event) > buf_end
			    || buf_ptr + sizeof(struct inotify_event) + event->len > buf_end) {
				fprintf(stderr, "error: partial/truncated message on inotify stream. bug!\n");
				fflush(stderr);
				abort(); /* I'd rather abort than have untestable recovery logic. */
			}

			handle_inotify_event(event);
		}
	}

err_2:
	close(inotify_fd);
err_1:
	return NULL;
}

int spawn_inotify_thread(void)
{
	int ret = 1; /* Error */

	/* Initialize the pthread attribute object */
	pthread_attr_t pthread_attr;
	errno = pthread_attr_init(&pthread_attr);
	if (0 != errno) {
		fprintf(stderr, "error: failed to initialize pthread_attr_t: %m\n");
		goto out_1;
	}

	/* Detached threads don't need to be joined. */
	errno = pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
	if (0 != errno) {
		fprintf(stderr, "error: failed to set thread detached state: %m\n");
		goto out_2;
	}

	/* Start thread */
	pthread_t tid;
	errno = pthread_create(&tid, &pthread_attr, inotify_thread, NULL);
	if (0 != errno) {
		fprintf(stderr, "error: failed to create thread: %m\n");
		goto out_2;
	}

	ret = 0; /* success */

out_2:
	pthread_attr_destroy(&pthread_attr);
out_1:
	return ret;
}

int main(int argc, char **argv)
{
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

out:
	return 0;
}
