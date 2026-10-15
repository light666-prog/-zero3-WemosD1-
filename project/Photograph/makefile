CXX = g++

CXXFLAGS = -Wall -g -O2 -std=c++14 \
           $(shell pkg-config --cflags opencv4 libavcodec libavformat libavutil libswscale)

LDFLAGS = -lpthread \
          $(shell pkg-config --libs opencv4 libavcodec libavformat libavutil libswscale)

SRCS = main.cpp camercapture.cpp EncodeStreamer.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf *.o $(TARGET)
