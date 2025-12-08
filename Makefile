CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC

TARGET = filecreate
LIB = libdb.so
SRC = filecreate.c

all: $(TARGET) $(LIB)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

$(LIB): $(SRC)
	$(CC) $(CFLAGS) -shared -o $(LIB) $(SRC)

clean:
	rm -f $(TARGET) $(LIB) *.o

.PHONY: all clean
help:
	@echo "Makefile for building $(TARGET) and $(LIB)"
	@echo "Targets:"
	@echo "  all    - Build both executable and shared library"
	@echo "  clean  - Remove built files"
	@echo "  help   - Show this help message"