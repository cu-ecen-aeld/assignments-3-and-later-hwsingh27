CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -g
LDFLAG = -pthread -lrt 
SRC = aesdsocket.c
OBJS = $(SRC:.c=.o)
TARGET = aesdsocket

all: $(TARGET)
default: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAG) $^ -o $@


.PHONY: clean

clean:
	$(RM) $(TARGET) *.o
