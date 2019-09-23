CXX := gcc
CXXFLAGS := -g3
CXXFLAGS_DEBUG := -g3
all: mcsp

mcsp: main.c graph.c graph.h utils.c utils.h threadpool.h threadpool.c
	$(CXX) $(CXXFLAGS) -Wall -std=c11 -o v4_iterative_par_c main.c graph.c graph.h utils.c utils.h threadpool.h threadpool.c -pthread

debug: main.c graph.c graph.h utils.c utils.h threadpool.h threadpool.c
	$(CXX) $(CXXFLAGS_DEBUG) -Wall -std=c11 -o v4_iterative_par_c main.c graph.c graph.h utils.c utils.h threadpool.h threadpool.c -pthread

clean:
	rm -f *.o
	rm -f v4_iterative_par_c
