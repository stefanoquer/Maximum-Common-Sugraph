/*
 * graph.c
 *
 *  Created on: Oct 9, 2018
 *      Author: gab
 */


#define _GNU_SOURCE

#include "graph.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void fail(const char* msg) {
    printf("%s\n", msg);
    exit(1);
}

unsigned int* calculate_degrees(graph_t *g) {
	short int size = g->n;
	unsigned int *degree = calloc(size, sizeof *degree);
	for (int v = 0; v < g->n; v++)
		for (int w = 0; w < g->n; w++)
			if (g->adjmat[v][w]) degree[v]++;
	return degree;
}

void add_edge(graph_t *g, int v, int w) {
    if (v != w) {
        g->adjmat[v][w] = 1;
        g->adjmat[w][v] = 1;
    } else {
        // To indicate that a vertex has a loop, we set its label to 1
        g->label[v] = 1;
    }
}

unsigned int read_word(FILE *fp) {
    unsigned char a[2];
    if (fread(a, 1, 2, fp) != 2)
        fail("Error reading file.\n");
    return (unsigned int)a[0] | (((unsigned int)a[1]) << 8);
}

// Precondition: *g is already zeroed out
// returns max edge label
void readBinaryGraph(char* filename, graph_t* g) {
    FILE* f;
	int i;
    if ((f=fopen(filename, "rb"))==NULL)
        fail("Cannot open file");

    unsigned int nvertices = read_word(f);
    g->n = nvertices;
    g->label = calloc(g->n, sizeof *g->label);
    g->adjmat = calloc(g->n, sizeof *g->adjmat);
    for(i = 0; i < g->n; i++)
    	g->adjmat[i] = calloc(g->n, sizeof *g->adjmat[i]);
    printf("%d vertices\n", nvertices);
    
    printf("paolo2");
    for (int i=0; i<nvertices; i++) {
        read_word(f);   // ignore label
    }

    for (int i=0; i<nvertices; i++) {
        int len = read_word(f);
        for (int j=0; j<len; j++) {
            int target = read_word(f);
            read_word(f); // ignore label
            add_edge(g, i, target);
        }
    }
	g->degree = calculate_degrees(g);
    fclose(f);
}

// Precondition: *g is already zeroed out
void readLadGraph(char* filename, graph_t* g) {
    FILE* f;
    int i;
    if ((f=fopen(filename, "r"))==NULL){
        free(g);
    	fail("Cannot open file");
    }
    int nvertices = 0, w;
    if (fscanf(f, "%d", &nvertices) != 1)
        fail("Number of vertices not read correctly.\n");
    g->n = nvertices;
    g->label = calloc(g->n, sizeof *g->label);
    g->adjmat = calloc(g->n, sizeof *g->adjmat);
    for(i = 0; i < g->n; i++)
    	g->adjmat[i] = calloc(g->n, sizeof *g->adjmat[i]);
    for (int i=0; i<nvertices; i++) {
        int edge_count;
        if (fscanf(f, "%d", &edge_count) != 1)
            fail("Number of edges not read correctly.\n");
        for (int j=0; j<edge_count; j++) {
            if (fscanf(f, "%d", &w) != 1)
                fail("An edge was not read correctly.\n");
            add_edge(g, i, w);
        }
    }
	g->degree = calculate_degrees(g);
    fclose(f);
}

void readGraph(char* filename, graph_t* g, char format) {
    if (format=='L') readLadGraph(filename, g);
    else if (format=='B') readBinaryGraph(filename, g);
    else fail("Unknown graph format\n");
}

graph_t *induced_subgraph(graph_t *g, int *vv) {
	graph_t * subg = calloc(1, sizeof *subg);
	subg->n = g->n;
	subg->label = calloc(g->n, sizeof *subg->label);
	subg->adjmat = calloc(g->n, sizeof *subg->adjmat);
	for (int n = 0; n < g->n; n++) subg->adjmat[n] = calloc(g->n, sizeof *subg->adjmat[n]);
	for (int i = 0; i < subg->n; i++)
		for (int j=0; j < subg->n; j++)
			subg->adjmat[i][j] = g->adjmat[vv[i]][vv[j]];
	for (int i=0; i<subg->n; i++)
		subg->label[i] = g->label[vv[i]];
	subg->degree = calculate_degrees(subg);
	return subg;
}

int graph_edge_count(graph_t *g) {
    int count = 0;
    for (int i=0; i<g->n; i++)
    	count += g->degree[i];
    return count;
}

void free_graph(graph_t *g){
	for(int i = 0; i < g->n; i++)
		free(g->adjmat[i]);
	free(g->adjmat);
	free(g->label);
	free(g->degree);
	free(g);
	return;
}

graph_t *sort_vertices_by_degree(graph_t *g, bool ascending ){
	int *vv = malloc(g->n * sizeof *vv );
	for (int i=0; i<g->n; i++) vv[i] = i;
	if (ascending) {
		INSERTION_SORT(int, vv, g->n, (g->degree[vv[j-1]] > g->degree[vv[j]]))
	} else {
		INSERTION_SORT(int, vv, g->n, (g->degree[vv[j-1]] < g->degree[vv[j]]))
	}

	graph_t *g_sorted = induced_subgraph(g, vv);
	free(vv);
	free_graph(g);
	return g_sorted;
}



