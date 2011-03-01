#include <stdio.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <sric.h>

Display *d;
sric_context ctx;

int
main(int argc, char **argv)
{
	sric_frame frame;
	sric_device *device;

	d = XOpenDisplay(NULL);
	if (d == NULL)
		abort();

	ctx = sric_init();
	if (ctx == NULL)
		abort();

	/* Find power board address */
	device = NULL;
	do {
		device = sric_enumerate_devices(ctx, device);
		if (device->type == SRIC_CLASS_POWER)
			break;
	} while (device != NULL)

	if (device == NULL) {
		fprintf(stderr, "Couldn't find a plugged in power board\n");
		abort();
	}

	/* Signal we want info on button presses */
#error bees

	XTestFakeKeyEvent(d, XK_Up, True, 0);
	XTestFakeKeyEvent(d, XK_Up, False, 20);
	XCloseDisplay(d);

	return 0;
}
