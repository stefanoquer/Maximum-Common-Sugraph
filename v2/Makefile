CXX := gcc
CXXFLAGS := -O3
CXXFLAGS_DEBUG := -g
all: mcsp

mcsp: main.c graph.c graph.h utils.c utils.h def.h threadpool.h threadpool.c
	$(CXX) $(CXXFLAGS) -Wall -std=c11 -o v2_trimble_par_c main.c graph.c graph.h utils.c utils.h def.h threadpool.h threadpool.c -pthread

debug: main.c graph.c graph.h utils.c utils.h def.h threadpool.h threadpool.c
	$(CXX) $(CXXFLAGS_DEBUG) -Wall -std=c11 -o v2_trimble_par_c main.c graph.c graph.h utils.c utils.h def.h threadpool.h threadpool.c -pthread

clean:
	rm -f *.o
	rm -f v2_trimble_par_c
