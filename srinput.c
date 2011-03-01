#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

int
main(int argc, char **argv)
{
	Display *d;

	d = XOpenDisplay(NULL);
	printf("Ohai, d is %p\n", d);
	XTestFakeKeyEvent(d, XK_Up, True, 0);
	XTestFakeKeyEvent(d, XK_Up, False, 20);
	XCloseDisplay(d);

	return 0;
}
