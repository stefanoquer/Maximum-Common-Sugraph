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

#ifndef TRIMBLE_IT_MULTI_UTILS_H
#define TRIMBLE_IT_MULTI_UTILS_H

#define _GNU_SOURCE
#define _POSIX_SOURCE

#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>


#include "graph.h"

#define L   0
#define R   1
#define LL  2
#define RL  3
#define ADJ 4
#define P   5
#define W   6
#define IRL 7

#define BDS 8



#define MIN(a, b) (a < b)? a : b

#define POOL_LEVEL 5
#define MAX_GRAPH_SIZE 64
#define DEFAULT_THREADS 8

uchar adjmat0[MAX_GRAPH_SIZE][MAX_GRAPH_SIZE], adjmat1[MAX_GRAPH_SIZE][MAX_GRAPH_SIZE], n0, n1;

typedef unsigned int uint;
typedef unsigned char uchar;

void *safe_realloc(void* old, uint new_size);

void uchar_swap(uchar *a, uchar *b);

uchar select_next_w(uchar *right, uchar *bd);

uchar select_next_v(uchar *left, uchar *bd);

void select_bidomain(uchar domains[][BDS], uint bd_pos, uchar *left, int current_matching_size, bool connected);

void update_incumbent(uchar cur[][2], uchar inc[][2], uchar cur_pos, uchar *inc_pos, uint th_idx);

// BIDOMAINS FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////////
void add_bidomain(uchar domains[][BDS], uint *bd_pos, uchar left_i, uchar right_i, uchar left_len, uchar right_len, uchar is_adjacent, uchar cur_pos);

uint calc_bound(uchar domains[][BDS], uint bd_pos, uint cur_pos, uint *bd_n);

uchar partition(uchar *arr, uchar start, uchar len, const uchar *adjrow);

void generate_next_domains(uchar domains[][BDS], uint *bd_pos, uint cur_pos, uchar *left, uchar *right, uchar v, uchar w, uint inc_pos);

bool check_sol(graph_t *g0, graph_t *g1, uchar sol[][2], uint sol_len);

double compute_elapsed_millisec(struct timespec start);

#endif //TRIMBLE_IT_MULTI_UTILS_H
