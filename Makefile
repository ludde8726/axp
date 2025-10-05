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
			-g -DCXP_DEBUG_ASSERTS
else
	CFLAGS := -Wall -Wextra -Wconversion -Wsign-conversion -Wsign-compare     \
			-Wfloat-equal -Wshadow -Wfloat-conversion                       \
			-std=c11 -pedantic -O2
endif

all: $(LIB_DIR)/libcxp.a

test: $(LIB_DIR)/libcxp.a testing.c
	$(CC) $(CFLAGS) -L$(LIB_DIR) -lcxp testing.c -o $(BIN_DIR)/testing
	./$(BIN_DIR)/testing

$(LIB_DIR)/libcxp.a: $(OBJ_DIR)/cxp.o | $(LIB_DIR)
	ar rcs $(LIB_DIR)/libcxp.a $(OBJ_DIR)/cxp.o

$(OBJ_DIR)/cxp.o: cxp.c cxp.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c cxp.c -o $(OBJ_DIR)/cxp.o

$(OBJ_DIR):
	mkdir -p $@

$(LIB_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@