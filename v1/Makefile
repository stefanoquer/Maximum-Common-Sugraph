CXX := gcc
CXXFLAGS := -g3
CXXFLAGS_DEBUG := -g
all: mcsp

mcsp: main.c graph.c graph.h
	$(CXX) $(CXXFLAGS) -Wall -std=c11 -o v1_trimble_seq_c main.c graph.c graph.h

debug: main.c graph.c graph.h
	$(CXX) $(CXXFLAGS_DEBUG) -Wall -std=c11 -o v1_trimble_seq_c main.c graph.c graph.h

clean:
	rm -f *.o
	rm -f v1_trimble_seq_c
