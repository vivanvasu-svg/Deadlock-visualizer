CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
SRCDIR = src
BINDIR = bin
TARGET = $(BINDIR)/deadlock

SRCS = $(SRCDIR)/deadlock.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(BINDIR)
