/*
 *  graphISO: Tools to compute the Maximum Common Subgraph between two graphs
 *  Copyright (c) 2019 Stefano Quer
 *  
 *  This program is free software : you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.If not, see < http: *www.gnu.org/licenses/>
 */

#ifndef GRAPH_H_
#define GRAPH_H_

#include <limits.h>
#include <stdbool.h>

#define INSERTION_SORT(type, arr, arr_len, swap_condition) do { \
		for (int i=1; i<arr_len; i++) {                             \
			for (int j=i; j>=1; j--) {                              \
				if (swap_condition) {                               \
					type tmp = arr[j-1];                            \
					arr[j-1] = arr[j];                              \
					arr[j] = tmp;                                   \
				} else {                                            \
					break;                                          \
				}                                                   \
			}                                                       \
		}                                                           \
} while(0);

typedef unsigned char uchar;

typedef struct graph_s {
    int n;
    unsigned char **adjmat;
    unsigned int *label;
    unsigned int *degree;
}graph_t;

unsigned int* calculate_degrees(graph_t *g);

graph_t *induced_subgraph(graph_t *g, int *vv);

int graph_edge_count(graph_t *g);

// Precondition: *g is already zeroed out
void readGraph(char* filename, graph_t* g, char format);

// Precondition: *g is already zeroed out
void readBinaryGraph(char* filename, graph_t* g);

// Precondition: *g is already zeroed out
void readLadGraph(char* filename, graph_t* g);

void free_graph(graph_t *g);

graph_t *sort_vertices_by_degree(graph_t *g, bool ascending );

#endif /* GRAPH_H_ */

