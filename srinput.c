#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <sric.h>

Display *d;
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
		return XK_Tab;
	case 2:
		return XK_Page_Up;
	case 4
		return XK_Page_Down;
	case 8:
	case 0x10:
		return 0;
	case 0x20:
		return XK_Right;
	case 0x40:
		return XK_Left;
	case 0x80:
		return XK_Up;
	case 0x100:
		return XK_Down;
	default:
		return 0;
	}
}

int
main(int argc, char **argv)
{
	sric_frame frame;
	const sric_device *device;
	int ret;

	d = XOpenDisplay(NULL);
	if (d == NULL) {
		fprintf(stderr, "Couldn't open X display\n");
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

		ret = sric_poll_rx(ctx, &frame, 100);

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

		/* And now do something with them */
		for (i = 0; i < 16; i++) {
			flag = 1 << i;
			if (flags & flag) {
				key = sric_flag_to_keysym(flag);
				XTestFakeKeyEvent(d, key, True, 0);
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

	/* And close display */
	XCloseDisplay(d);

	return 0;
}
