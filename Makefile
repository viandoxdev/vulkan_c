Q=@
CC=gcc

GCCCFLAGS=-Wno-format-truncation 
CFLAGS=-std=c11 -pedantic -g -Wall
LDFLAGS=-lm

BUILD_DIR=./objects
BIN=ast

RUNARGS=

SOURCES=$(wildcard *.c)

OBJECTS:=$(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

ifeq ($(CC),gcc)
	CFLAGS:=$(CFLAGS) $(GCCCFLAGS)
endif

.PHONY: run
run: $(BIN)
	@echo "RUN   $(BIN) $(RUNARGS)"
	$(Q) chmod +x $(BIN)
	$(Q) ./$(BIN) $(RUNARGS)

$(BIN): $(OBJECTS)
	@echo "LD    $@"
	$(Q) $(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "CC    $<"
	$(Q) $(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(BUILD_DIR)
$(BUILD_DIR):
	@echo "MKDIR"
	$(Q) mkdir -p $(BUILD_DIR)

.PHONY: clean
clean:
	@echo "CLEAN"
	$(Q) rm -fr $(BUILD_DIR)
	$(Q) rm -fr $(BIN)

