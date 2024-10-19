CC = gcc
CFLAGS = -I$(INCLUDE_DIR) -Wall -Wextra -pedantic -std=c99
INCLUDE_DIR = include
SRC_DIR = src
OBJECTS = main.o append_buffer.o

vpath %.h $(INCLUDE_DIR)
vpath %.c $(SRC_DIR)

steqs: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS)

$(OBJECTS): append_buffer.h util.h

.PHONY: clean
clean:
	rm steqs $(OBJECTS) 



