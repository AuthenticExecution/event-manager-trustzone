CFLAGS = -Wall -DDBG
SRCS=$(wildcard host/*.c)
OBJS=$(SRCS:.c=.o)

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

all: $(OBJS)
	gcc $(CFLAGS) -o app $(OBJS)

run: all
	./app

clean:
	rm -rf $(OBJS) app