CFLAGS=-c -Wall
LDFLAGS=-L.
LEGACYSOURCES=
SOURCES=main.cpp
OBJECTS=$(SOURCES:.cpp=.o) $(LEGACYSOURCES:.c=.o)
EXECUTABLE=testApp
G++=g++
all: $(SOURCES) $(EXECUTABLE)
$(EXECUTABLE): $(OBJECTS)
	g++ --std=c++0x -pthread -g $(OBJECTS) $(LDFLAGS) -Wl,-rpath . -o $@
.c.o:
	gcc -g $(CFLAGS) $< -o $@
.cpp.o:
	g++ -std=c++0x -g $(CFLAGS) $< -o $@

