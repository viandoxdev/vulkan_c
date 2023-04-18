Q=
CC=gcc
GLSLC=glslc

GCCCFLAGS=-Wno-format-truncation #-fsanitize=address
CFLAGS=-std=c2x -pedantic -g -Wall
LDFLAGS=-lm -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

# disable logging
# CFLAGS+=-DLOG_DISABLE

# flush logs
# CFLAGS+=-DLOG_FLUSH

# enable validation layers
CFLAGS+=-DENABLE_VALIDATION_LAYERS

BUILD_DIR=./objects
BIN=ast

RUNARGS=

SOURCES=$(wildcard *.c)
INCLUDES_STR=
INCLUDES_BYTES=shader.vert.spv shader.frag.spv

OBJECTS:=$(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
EXPANDED:=$(patsubst %,$(BUILD_DIR)/%,$(SOURCES))
DEPS:=$(patsubst %.c,$(BUILD_DIR)/%.d,$(SOURCES))
INCLUDES:=$(patsubst %,./include/%.str,$(INCLUDES_STR)) $(patsubst %,./include/%.bytes,$(INCLUDES_BYTES))
HEADERS:=$(wildcard *.h)
ASM:=$(patsubst %.c,$(BUILD_DIR)/%.s,$(SOURCES))

ifeq ($(CC),gcc)
	CFLAGS:=$(CFLAGS) $(GCCCFLAGS)
endif

.PHONY: run
run: $(BIN)
	@echo "RUN   $(BIN) $(RUNARGS)"
	$(Q) chmod +x $(BIN)
	@echo
	$(Q) ./$(BIN) $(RUNARGS)
.PHONY: build
build: $(BIN)

asm: $(INCLUDES) $(ASM)

expand: $(INCLUDES) $(EXPANDED)

$(BIN): $(INCLUDES) $(OBJECTS)
	@echo "LD    $@"
	$(Q) $(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

-include $(DEPS)

# not necessary, can be removed.
.PRECIOUS: $(BUILD_DIR)/%.vert.spv $(BUILD_DIR)/%.frag.spv

$(BUILD_DIR)/%.vert.spv: %.vert | $(BUILD_DIR)
	@echo "CC    $<"
	$(Q) $(GLSLC) $< -o $@
$(BUILD_DIR)/%.frag.spv: %.frag | $(BUILD_DIR)
	@echo "CC    $<"
	$(Q) $(GLSLC) $< -o $@

./include/%.str: % | ./include
	@echo "STR   $<"
	$(Q) sed 's/\\/\v/g;s/"/\\"/g;s/\t/\\t/g;s/\v/\\\\/g;s/\(.*\)/"\1\\n"/' < $< > $@
./include/%.bytes: % | ./include
	@echo "BYTES $<"
	$(Q) xxd -i $< | head -n-2 | tail -n+2 > $@
./include/%.bytes: $(BUILD_DIR)/% | ./include
	@echo "BYTES $<"
	$(Q) xxd -i $< | head -n-2 | tail -n+2 > $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "CC    $<"
	$(Q) $(CC) -MMD $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.s: %.c $(BUILD_DIR)/%.o
	@echo "ASM   $<"
	$(Q) $(CC) $(CFLAGS) -S $< -o $@
$(BUILD_DIR)/%.c: %.c $(BUILD_DIR)/%.o
	@echo "CPP   $<"
	$(Q) $(CC) $(CFLAGS) -E $< | grep -v "^#" | clang-format > $@

.PRECIOUS: $(BUILD_DIR)
$(BUILD_DIR):
	@echo "MKDIR $@"
	$(Q) mkdir -p $(BUILD_DIR)

.PRECIOUS: ./include
./include:
	@echo "MKDIR $@"
	$(Q) mkdir -p ./include

.PHONY: clean
clean:
	@echo "CLEAN"
	$(Q) rm -fr $(BUILD_DIR)
	$(Q) rm -fr ./include
	$(Q) rm -fr $(BIN)

