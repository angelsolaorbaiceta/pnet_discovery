CC=gcc
CFLAGS=-Wall -Werror

OBJ_DIR=obj
BIN_DIR=bin

PROGRAM=$(BIN_DIR)/pnet_broadcast

.PHONY: clean

$(PROGRAM): $(OBJ_DIR)/broadcast.o $(OBJ_DIR)/main.o | $(BIN_DIR)
	$(CC) -o $@ $^

$(OBJ_DIR)/main.o: main.c | $(OBJ_DIR)
	$(CC) $(FLAGS) -o $@ -c $<

$(OBJ_DIR)/broadcast.o: broadcast.c broadcast.h | $(OBJ_DIR)
	$(CC) $(FLAGS) -o $@ -c $< -pthread

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)


