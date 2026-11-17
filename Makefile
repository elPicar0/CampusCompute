CC      := gcc
CFLAGS  := -Wall -Wextra -g -Iinclude
LDFLAGS := -pthread

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lrt
endif

SRC_DIR := src
INC_DIR := include
OBJ_DIR := build
TARGET  := campus_os

SRCS := $(wildcard $(SRC_DIR)/*.c)
HDRS := $(wildcard $(INC_DIR)/*.h)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# only main.c keeps its main(); every other module's standalone main() is renamed
# so each file can still be compiled and tested on its own
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c $(HDRS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -Dmain=module_main_$* -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: $(TARGET)
	./$(TARGET)

# automated checks (tests/run_tests.sh)
test: $(TARGET)
	./tests/run_tests.sh

# removes build output, the executable and generated files (not anyone's IPC objects)
clean:
	rm -rf $(OBJ_DIR) $(TARGET) temp_worker.log campus.log
