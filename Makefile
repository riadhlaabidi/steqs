CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
SRC_DIR = src
OBJECTS = main.o append_buffer.o editor.o kbd.o

vpath %.h $(SRC_DIR)
vpath %.c $(SRC_DIR)

steqs: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

.PHONY: clean
clean:
	rm steqs $(OBJECTS) 



