FILES = lib/ae/ae.c lib/utf8_decode.c lib/cJSON.c buffer.c socket.c sa.c dubbo_hessian.c dubbo_codec.c dubbo_client.c dubbo.c
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer

dubbo: $(FILES)
	$(CC) -D_GNU_SOURCE -std=gnu99 -g -Wall -o $@ $^

dubbo_test: $(FILES)
	$(CC) -D_GNU_SOURCE -std=gnu99 -O0 -g3 -Wall -o $@ $^

dubbo_debug: $(FILES)
	$(CC) -fsanitize=address -fno-omit-frame-pointer -D_GNU_SOURCE -std=gnu99 -g3 -O0 -Wall $(ASAN_FLAGS) -o $@ $^

.PHONY: clean
clean:
	-/bin/rm -f dubbo
	-/bin/rm -f dubbo_debug
	-/bin/rm -f dubbo_test
	-/bin/rm -rf *.dSYM