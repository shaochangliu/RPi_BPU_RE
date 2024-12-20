CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lelf

TARGET = time_diff
OBJS = time_diff.o

all: $(TARGET) branch.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

time_diff.o: time_diff.c
	$(CC) $(CFLAGS) -c $<

branch.o: branch.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS) branch.o

.PHONY: all clean