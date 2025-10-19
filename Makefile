CC := gcc
BUILD_DIR := build
LIB_DIR := $(BUILD_DIR)/lib
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

DEBUG ?= 1

ifeq ($(DEBUG),1)
	CFLAGS := -Wall -Wextra -Wconversion -Wsign-conversion -Wsign-compare     \
			-Wfloat-equal -Wshadow -Wfloat-conversion                       \
			-std=c11 -pedantic										      \
			-fsanitize=address,float-divide-by-zero,signed-integer-overflow \
			-g -DAXP_DEBUG_ASSERTS
else
	CFLAGS := -Wall -Wextra -Wconversion -Wsign-conversion -Wsign-compare     \
			-Wfloat-equal -Wshadow -Wfloat-conversion                       \
			-std=c11 -pedantic -O2
endif

all: $(LIB_DIR)/libaxp.a

test: $(LIB_DIR)/libaxp.a testing.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -L$(LIB_DIR) -laxp testing.c -o $(BIN_DIR)/testing
	./$(BIN_DIR)/testing

clean:
	rm -rf $(BUILD_DIR)

# I know this is not necessary but i am doing it in case i need to add more files later on.
$(LIB_DIR)/libaxp.a: $(OBJ_DIR)/axp.o | $(LIB_DIR)
	ar rcs $(LIB_DIR)/libaxp.a $(OBJ_DIR)/axp.o

$(OBJ_DIR)/axp.o: axp.c axp.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c axp.c -o $(OBJ_DIR)/axp.o

$(OBJ_DIR):
	mkdir -p $@

$(LIB_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@