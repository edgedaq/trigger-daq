# Makefile — exclude examples/ from build
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -I. -Iprotocol
LDFLAGS := -lm

# Platform extras
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -D_GNU_SOURCE
endif
# Windows/MSYS/MinGW: link winsock
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
else ifneq (,$(findstring MINGW,$(UNAME_S)))
    LDFLAGS += -lws2_32
else ifneq (,$(findstring MSYS,$(UNAME_S)))
    LDFLAGS += -lws2_32
endif

# ===== Build folder =====
BUILD_DIR := build

# ===== Sources (exclude examples/) =====
# Preferred: use find (works on Linux/macOS/MSYS2)
SRCS_FIND := $(shell find . -type f -name '*.c' ! -path './examples/*')

# Fallback if find yields empty (extremely minimal env): only root + protocol
ifeq ($(strip $(SRCS_FIND)),)
  SRCS := $(wildcard ./*.c) $(wildcard protocol/*.c)
else
  SRCS := $(SRCS_FIND)
endif

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ===== Target =====
TARGET := device_simulator
BIN    := $(BUILD_DIR)/$(TARGET)

# ===== Default =====
all: $(BIN)

# Link
$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile: all objects and deps to build/
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Auto deps
-include $(DEPS)

.PHONY: all clean debug
