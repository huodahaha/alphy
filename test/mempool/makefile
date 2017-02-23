# huodahaha 2/24/2017

CC = gcc
SRC_DIR = ../../src
TARGET = test
SRC = $(SRC_DIR)/mempool.c \
	  ./*.c
		
OBJS = $(SRCS:.c=.o)
INC = -I $(SRC_DIR)
CFLAGS = -Wall -std=gnu99 -g
LFLAGS = -lpthread
MACROS = -D DEBUG_MEMPOOL DEBUG_RINGBUFFER

all:
	$(CC) $(CFLAGS) $(LFLAGS) $(SRC) $(INC) -o $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS)
