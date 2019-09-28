#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // IWYU pragma: keep - required for abort
#include <sys/inotify.h>
#include <unistd.h>

#include "device_thread.h"

#include "inotify_thread.h"

static const char * const dev_input_path = "/dev/input";

static int is_event_filename(char *name)
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
			spawn_device_thread(path);
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
	spawn_device_thread(path);
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
