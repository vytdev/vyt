CC = gcc
CFLAGS = -std=c11 -Wall -pedantic -MMD -MP
LDFLAGS =
TARGET = vyt
SRC = $(shell find src -name '*.c' -type f)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
DEP = $(patsubst src/%.c,build/%.d,$(SRC))

all: $(TARGET)

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

-include $(DEP)

.PHONY: clean debug perf

clean:
	rm -rf $(TARGET) build

debug: CFLAGS += -g -D__DEBUG
debug: LDFLAGS += -g
debug: all

perf: CFLAGS += -O3 -march=native -mtune=native -fomit-frame-pointer \
	-funroll-loops -finline-functions
perf: all
