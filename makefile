override CFLAGS := -Wall -Werror -std=gnu99 -pedantic -O0 -g -pthread $(CFLAGS)
override LDLIBS := -pthread $(LDLIBS)

tls.o: tls.c

test: tls.o test.c

.PHONY: clean

clean:
	rm -f tls.o
