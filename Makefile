# Add include/lib paths to work, not to be pretty.
LIBS+=		-lX11 -lXtst -lsric -L../sricd/libsric
INCLUDES+=	-I../sricd/libsric

srinput: srinput.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) $(LIBS) $^ -o $@
