#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <sric.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

int evdev_fd;
sric_context ctx;
bool quit = false;

/* The time that we last received an input from one of the rotary encoders.
 * Ignore all rot inputs that happen for a short period of time after that,
 * as it turns out the encoders can fire in bursts :| */
#define ROT_TIMEOUT	25*1000		/* 25ms between firings */
struct timeval rot_act_time[2];

static inline int
is_rot_flag(flag)
{

	if (flag == 0x20 || flag == 0x40)
		return 0;

	if (flag == 0x80 || flag == 0x100)
		return 1;

	return -1;
}

static inline bool
discard_rot_burst(int flag, struct timeval *now)
{
	int lasttime, nowtime;
	int idx;

	idx = is_rot_flag(flag);
	if (idx < 0)
		return false;

	/* So, compare time with when we last received an input... */
	if (now->tv_sec > rot_act_time[idx].tv_sec)
		nowtime = 1000000;
	else
		nowtime = 0;

	lasttime = rot_act_time[idx].tv_usec;
	lasttime += ROT_TIMEOUT;
	nowtime += now->tv_usec;

	if (nowtime > lasttime) {
		/* Update last time */
		memcpy(&rot_act_time[idx], now, sizeof(*now));
		return false;
	}

	return true;
}

int
sric_flag_to_keysym(int flag)
{

	switch (flag) {
	case 1:
		return KEY_TAB;
	case 2:
		return KEY_PAGEUP;
	case 4:
		return KEY_PAGEDOWN;
	case 8:
	case 0x10:
		return KEY_RESERVED;
	case 0x20:
		return KEY_RIGHT;
	case 0x40:
		return KEY_LEFT;
	case 0x80:
		return KEY_UP;
	case 0x100:
		return KEY_DOWN;
	default:
		return KEY_RESERVED;
	}
}

int
main(int argc, char **argv)
{
	struct uinput_user_dev dev;
	sric_frame frame;
	struct timeval now;
	const sric_device *device;
	int ret, k;

	evdev_fd = open("/dev/input/uinput", O_RDWR, 0);
	if (evdev_fd < 0) {
		perror("Couldn't open userland input device");
		abort();
	}

	/* Turns out there's some setup activity: */
	memset(&dev, 0, sizeof(dev));
	strcpy(dev.name, "powerboard");
	dev.id.bustype = 0xFACE;
	dev.id.vendor = 0xBEE5;
	dev.id.product = 0xFACE;
	dev.id.version = 0xBEE5;
	if (write(evdev_fd, &dev, sizeof(dev)) != sizeof(dev)) {
		perror("Short write when writing input dev info");
		abort();
	}

	if (ioctl(evdev_fd, UI_SET_EVBIT, EV_KEY) < 0) {
		perror("Couldn't identify sr input as a keyboard");
		abort();
	}

	for (k = KEY_RESERVED; k <= KEY_UNKNOWN; k++) {
		if (ioctl(evdev_fd, UI_SET_KEYBIT, k) < 0) {
			perror("Couldn't set a keyboard ID bit");
			abort();
		}
	}

	if (ioctl(evdev_fd, UI_DEV_CREATE, NULL) < 0) {
		perror("Couldn't create user-input input device");
		abort();
	}

	ctx = sric_init();
	if (ctx == NULL) {
		fprintf(stderr, "Couldn't open sricd\n");
		abort();
	}

	/* Find power board address */
	device = NULL;
	do {
		device = sric_enumerate_devices(ctx, device);
		if (device->type == SRIC_CLASS_POWER)
			break;
	} while (device != NULL);

	if (device == NULL) {
		fprintf(stderr, "Couldn't find a plugged in power board\n");
		abort();
	}

	ret = sric_note_register(ctx, device->address, 0);
	if (ret != 0) {
		fprintf(stderr, "Couldn't register interest in input note: %d\n"
							, sric_get_error(ctx));
		abort();
	}

	/* Tell board we want info on button presses */
	frame.address = device->address;
	frame.note = -1;
	frame.payload_length = 2;
	frame.payload[0] = 5; /* enable notes cmd */
	frame.payload[1] = 1;

	ret = sric_txrx(ctx, &frame, &frame, -1);
	if (ret != 0) {
		fprintf(stderr, "Couldn't start notes: %d\n",
					sric_get_error(ctx));
		abort();
	}

	/* All is now fine and wonderful. Receive notifications about button
	 * presses and post X events describing them */

	/* Smooth out rot encoder bursts */
	gettimeofday(&rot_act_time[0], NULL);
	gettimeofday(&rot_act_time[1], NULL);

	while (quit == false) {
		int i, flag, key;
		uint16_t flags, edges;

		ret = sric_poll_note(ctx, &frame, 100);

		if (ret != 0 && sric_get_error(ctx) != SRIC_ERROR_TIMEOUT) {
			fprintf(stderr, "Error getting input note: %d\n",
							sric_get_error(ctx));
			break;
		} else if (ret != 0 && sric_get_error(ctx)==SRIC_ERROR_TIMEOUT){
			continue;
		}

		/* Unpack flags/edges. Sent little endian uint16_ts */
		if (frame.payload_length != 5) {
			fprintf(stderr, "Invalid length %d for input note\n",
						frame.payload_length);
			break;
		}

		flags = frame.payload[1];
		flags |= frame.payload[2] << 8;
		edges = frame.payload[3];
		edges |= frame.payload[4] << 8;

		printf("flags %X edges %X\n", flags, edges);

		gettimeofday(&now, NULL);

		/* And now do something with them */
		for (i = 0; i < 16; i++) {
			struct input_event evt;

			flag = 1 << i;
			if (flags & flag) {
				key = sric_flag_to_keysym(flag);

				if (discard_rot_burst(flag, &now))
					continue;

printf("key 0x%X sent\n", key);
				memcpy(&evt.time, &now, sizeof(now));
				evt.type = EV_KEY;
				evt.code = key;
				evt.value = 1;

				if (is_rot_flag(flag) < 0) {

					/* Pressed/released? */
					if (flag & edges) {
						evt.value = 1;
					} else {
						evt.value = 0;
					}

					write(evdev_fd, &evt, sizeof(evt));
				} else {
					/* Rot input, send press then release
					 * event */
					write(evdev_fd, &evt, sizeof(evt));
					evt.value = 0;
					write(evdev_fd, &evt, sizeof(evt));
				}

				evt.type = EV_SYN;
				evt.code = SYN_REPORT;
				evt.value = 0;
				write(evdev_fd, &evt, sizeof(evt));
			}
		}
	}

	/* Turn off notes */
	frame.address = device->address;
	frame.note = -1;
	frame.payload_length = 2;
	frame.payload[0] = 5;
	frame.payload[1] = 0;
	sric_txrx(ctx, &frame, &frame, -1);

	/* Unregister from note */
	sric_note_unregister(ctx, device->address, 0);

	return 0;
}
