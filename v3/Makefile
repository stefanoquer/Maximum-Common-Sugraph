CXX := gcc
CXXFLAGS := -O3
CXXFLAGS_DEBUG := -g
all: mcsp

mcsp: main.c graph.c graph.h
	$(CXX) $(CXXFLAGS) -Wall -std=c11 -o v3_iterative_seq_c main.c graph.c graph.h

debug: main.c graph.c graph.h
	$(CXX) $(CXXFLAGS_DEBUG) -Wall -std=c11 -o v3_iterative_seq_c main.c graph.c graph.h

clean:
	rm -f *.o
	rm -f v3_iterative_seq_c
