CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lelf

TARGET = associativity
OBJS = associativity.o

all: $(TARGET) branch.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

associativity.o: associativity.c
	$(CC) $(CFLAGS) -c $<

branch.o: branch.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS) branch.o

.PHONY: all clean