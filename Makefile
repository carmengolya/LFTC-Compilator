CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2 -Isrc
LDFLAGS  :=

TARGET   := compiler
TESTFILE := src/tests/testlex.c
OUTFILE  := src/tests/testlex.out
BUILD    := build

# toate .c din src, dar fara src/tests
SRC := $(filter-out src/tests/%, $(wildcard src/*.c src/*/*.c))
OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# compile rule + creare directoare in build/ (ex: build/ si build/subdir/)
$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(TESTFILE) > $(OUTFILE)

clean:
	rm -rf $(BUILD) $(TARGET)