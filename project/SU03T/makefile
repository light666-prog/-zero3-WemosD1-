CC=gcc
CFLAGS= -Wall -g
OBJS= main.o Control_led.o  oledi2c.o aht20.o  Usb.o mq2.o buzzer.o SU03T.o
TARGET=test
all:$(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) -lwiringPi -lm -lpthread

.PHONY clean:
	rm -rf *.o  $(TARGET)
