CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lelf

TARGET = index
OBJS = index.o

all: $(TARGET) branch.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

index.o: index.c
	$(CC) $(CFLAGS) -c $<

branch.o: branch.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS) branch.o

.PHONY: all clean