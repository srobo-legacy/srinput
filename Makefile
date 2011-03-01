LDFLAGS+=	-lX11 -lXtst

srinput: srinput.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
