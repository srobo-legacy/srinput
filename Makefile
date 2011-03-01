LDFLAGS+=	-lX11

srinput: srinput.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
