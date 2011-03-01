# Add include/lib paths to work, not to be pretty.
LDFLAGS+=	-lX11 -lXtst -lsric -L../sricd/libsric
CFLAGS+=	-I../sricd/libsric

srinput: srinput.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
