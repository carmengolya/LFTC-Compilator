CC          := gcc
CFLAGS      := -Wall -Wextra -std=c11 -O2 -Isrc
LDFLAGS     :=
VALGRIND    := valgrind --leak-check=full

TARGET      := compiler
PARSER_IN   := src/tests/testparser.c
PARSER_OUT  := src/tests/testparser.out
LEXER_IN    := src/tests/testlex.c
LEXER_OUT   := src/tests/testlex.out
DOMAIN_IN   := src/tests/testad.c
DOMAIN_OUT  := src/tests/testad.out
BUILD       := build

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
	./$(TARGET) $(PARSER_IN) > $(PARSER_OUT)
	./$(TARGET) $(LEXER_IN)  > $(LEXER_OUT)
	./$(TARGET) $(DOMAIN_IN) > $(DOMAIN_OUT)

mem_check: $(TARGET)
	$(VALGRIND) ./$(TARGET)

clean:
	rm -rf $(BUILD)      $(TARGET)
	rm -f  $(PARSER_OUT) $(LEXER_OUT) $(DOMAIN_OUT)