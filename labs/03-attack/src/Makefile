CC = gcc
CFLAGS = -c

SRCS := $(wildcard *.s)
BINS := $(SRCS:%.s=%.bin)

all: $(BINS)

%.bin: %.o
	objcopy -O binary $< $@

%.o: %.s
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.o *.bin
