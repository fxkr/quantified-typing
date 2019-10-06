#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>

#include "dev_input_set.h"
#include "stats_thread.h"
#include "util.h"

#include "device_thread.h"

struct device_thread_data {
	char *path;
	struct libevdev *dev;
	struct timespec last_time_mono;
};

static void device_thread_data_free(struct device_thread_data *h)
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

static void device_thread_handle_event(struct device_thread_data *thread, const struct input_event *event)
{
	if (!libevdev_event_is_type(event, EV_KEY)) {
		return;
	}

	/* Not a key down event? */
	if (event->value != 1) {
		return;
	}

	struct timespec cur_time;
	clock_gettime(CLOCK_MONOTONIC, &cur_time);

	/* check for time warps */
	if (cur_time.tv_sec < thread->last_time_mono.tv_sec ||
	    (cur_time.tv_sec == thread->last_time_mono.tv_sec && cur_time.tv_nsec < thread->last_time_mono.tv_nsec)) {
		/* should never happen, but kernel bugs have caused this before */
		fprintf(stderr, "fatal: CLOCK_MONOTONIC jumped backwards\n");
		abort();
	}

	struct timespec delta_time;
	timespec_subtract(&delta_time, &cur_time, &thread->last_time_mono);

	memcpy(&thread->last_time_mono, &cur_time, sizeof(struct timespec));

	clock_gettime(CLOCK_MONOTONIC, &thread->last_time_mono);

	stats_thread_submit_key(&cur_time, &delta_time);
}

static void *device_thread(void *arg)
{
	struct device_thread_data *thread = (struct device_thread_data *)arg;
	struct input_event event;
	int rc;

	fprintf(stderr, "info: attached to %s (%s)\n", thread->path, libevdev_get_name(thread->dev));

	do {
		rc = libevdev_next_event(
				thread->dev,
				LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING,
				&event);

		if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
			device_thread_handle_event(thread, &event);
		}

	} while (rc == LIBEVDEV_READ_STATUS_SYNC
			|| rc == LIBEVDEV_READ_STATUS_SUCCESS
			|| rc == -EAGAIN);

	device_thread_data_free(thread);

	return NULL;
}

void spawn_device_thread(char *path)
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

	struct device_thread_data *thread = calloc(sizeof(*thread), 1);
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
	errno = pthread_create(&tid, &pthread_attr, device_thread, thread);
	if (0 != errno) {
		fprintf(stderr, "error: failed to create thread: %m\n");
		goto err_4;
	}

	pthread_attr_destroy(&pthread_attr);

	return; /* Success */

err_4:
	pthread_attr_destroy(&pthread_attr);
err_3:
	device_thread_data_free(thread);
err_2:
	close(fd);
err_1:
	return; /* Error */
}


