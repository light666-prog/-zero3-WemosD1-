CC=gcc
CFLAGS= -Wall -g
OBJS=tencent.o db.o
TARGET=tencent
all:$(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)  -lssl -lcrypto -lcurl -lsqlite3 -lcjson

.PHONY clean:
	rm -rf *.o  $(TARGET)
