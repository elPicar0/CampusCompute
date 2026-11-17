CC      ?= gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Wpedantic -g -O1 -pthread -Iinclude
LDFLAGS := -pthread
LDLIBS  := -lrt

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:src/%.c=build/%.o)
BIN  := campus

.PHONY: all clean test cleanup-ipc

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

test: $(BIN)
	./tests/run_tests.sh

cleanup-ipc: $(BIN)
	./$(BIN) cleanup

clean:
	rm -rf build $(BIN) logs

-include $(OBJS:.o=.d)
