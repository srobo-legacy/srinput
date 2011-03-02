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

void
signal_handler(int sig)
{

	quit = true;
	return;
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
	const sric_device *device;
	int ret, k;

	evdev_fd = open("/dev/input/uinput", O_RDWR, 0);
	if (evdev_fd < 0) {
		perror("Couldn't open userland input device");
		abort();
	}

	/* Turns out there's some setup activity: */
	memset(&dev, 0, sizeof(dev));
	strcpy(dev.name, "sr-input");
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

	/* Don't quit on signals - instead, shut down gracefully, telling the
	 * power board to stop sending notes */

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

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

		printf("flags %X\n", flags);

		/* And now do something with them */
		for (i = 0; i < 16; i++) {
			struct input_event evt;

			flag = 1 << i;
			if (flags & flag) {
				key = sric_flag_to_keysym(flag);
printf("key 0x%X sent\n", key);
				gettimeofday(&evt.time, NULL);
				evt.type = EV_KEY;
				evt.code = key;
				evt.value = 1;
/* Send some input events */

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
