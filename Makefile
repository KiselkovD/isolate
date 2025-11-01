CC = gcc
CFLAGS = -Iinclude
SRC_DIR = src
OBJ_DIR = obj
TARGET = isolate

SRC = $(SRC_DIR)/isolate.c $(SRC_DIR)/netns.c $(SRC_DIR)/cgroup_control.c
OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
