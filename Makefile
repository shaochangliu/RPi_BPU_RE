CC = gcc
CFLAGS = -Wall

TARGET = time_diff
OBJS = time_diff.o branch.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

time_diff.o: time_diff.c
	$(CC) $(CFLAGS) -c $<

branch.o: branch.s
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean