CC      = gcc
# -fPIC is required for compiling object files that go into a shared library
CFLAGS  = -Wall -Wextra -g -fPIC

# Source files for the main executable
SRCS    = main.c schema.c storage.c pager.c query.c
OBJS    = $(SRCS:.c=.o)
TARGET  = filecreate

# Source files for the shared library
LIB_SRCS = schema.c storage.c pager.c query.c
LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB_TGT  = libdb.so

# 'all' now builds both the main executable and the shared library
all: $(TARGET) $(LIB_TGT)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to build the shared library
$(LIB_TGT): $(LIB_OBJS)
	$(CC) -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up both targets and all generated object files
clean:
	rm -f *.o $(TARGET) $(LIB_TGT) new_file1

.PHONY: all clean