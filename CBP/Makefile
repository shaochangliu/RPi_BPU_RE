CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lelf

TARGET = ghr_len
OBJS = ghr_len.o

all: $(TARGET) branch.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

ghr_len.o: ghr_len.c
	$(CC) $(CFLAGS) -c $<

branch.o: branch.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS) branch.o

.PHONY: all clean