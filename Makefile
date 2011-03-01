LDFLAGS+=	-lX11 -lXtst
# Add sric.h include path. To work, not to be pretty.
CFLAGS+=	-I../sricd/libsric

srinput: srinput.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
