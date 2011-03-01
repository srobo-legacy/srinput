#include <stdio.h>

#include <X11/Xlib.h>

int
main(int argc, char **argv)
{
	Display *d;

	d = XOpenDisplay(NULL);
	printf("Ohai, d is %p\n", d);
	XCloseDisplay(d);

	return 0;
}
