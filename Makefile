#----------------------------------------------------
XML_INCLUDE = $(shell xml2-config --cflags)
XML_LIBRARY = $(shell xml2-config --libs)
LDFLAGS = -lpthread $(XML_LIBRARY)
CXX = g++
CXXFLAGS = -g -Wall $(XML_INCLUDE)
#----------------------------------------------------
TARGET = elevator
OBJS = bsem.o elevator.o
#----------------------------------------------------
all:$(TARGET)

$(TARGET):$(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o $(TARGET)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm $(TARGET) -rf
	rm $(OBJS) -rf
#----------------------------------------------------
