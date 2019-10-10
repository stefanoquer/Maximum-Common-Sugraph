#
#  graphISO: Tools to compute the Maximum Common Subgraph between two graphs
#  Copyright (c) 2019 Stefano Quer
#  
#  This program is free software : you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.If not, see < http: #www.gnu.org/licenses/>
#

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
