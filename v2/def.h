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

#ifndef DEF_H_
#define DEF_H_

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

#define SPLIT_LEVEL 6

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))



typedef atomic_uint atomic_incumbent; // @suppress("Type cannot be resolved")

//atomic_incumbent global_incumbent; // @suppress("Type cannot be resolved")

typedef struct vtx_pair_s {
	int v;
	int w;
}vtx_pair_t;

typedef struct vtx_pair_list_s {
	vtx_pair_t *vals;
	unsigned len;
	unsigned size;
}vtx_pair_list_t;

typedef struct bidomain_s {
	int l,     r;
	unsigned  left_len,     right_len;
	bool is_adjacent;
} bidomain_t;

typedef struct bidomain_list_s {
	bidomain_t *vals;
	unsigned len;
	unsigned size;
}bidomain_list_t;

typedef struct position_s{
	int vals[SPLIT_LEVEL +1];
	int depth;
}position_t;

typedef struct main_args_s{
	int depth;
	graph_t *g0;
	graph_t *g1;

	vtx_pair_list_t **per_thread_incumbents;
	vtx_pair_list_t *current;
	bidomain_list_t *domains;
	int *left, *right;

	atomic_uint *global_incumbent;// @suppress("Type cannot be resolved")


	position_t pos;
	struct help_me_s *help_me;
	int thread_idx;

	atomic_uint *shared_i; // @suppress("Type cannot be resolved")
	int i_end;
	int bd_idx;
	bidomain_t *bd;
	int next_i;
}args_t;


typedef void (*func)(args_t* d);


typedef struct task_s{
	func func;
	int pending;
	args_t* args;
}task_t;



typedef struct node_s{
	task_t *task;
	position_t pos;
	struct node_s *next;
}node_t;


typedef struct help_me_s{
	pthread_t *threads;
	unsigned int n_threads;

	atomic_bool  finish; // @suppress("Type cannot be resolved")

	task_t **tasks;
	node_t *head, *tail;
	int task_num;
	struct params_s ** params;

	pthread_mutex_t general_mtx;
	pthread_cond_t cv;
} threadpool_t;

typedef struct params_s{
	threadpool_t *help_me;
	int idx;
}param_t;



#endif /* DEF_H_ */
