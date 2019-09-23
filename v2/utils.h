/*
 * utils.h
 *
 *  Created on: Oct 12, 2018
 *      Author: gab
 */

#ifndef UTILS_H_
#define UTILS_H_

#include "def.h"
/* atomic incumbent functions */
void init_global_incumbent(atomic_incumbent *global_incumbent);
void update_global_incumbent(atomic_incumbent *global_incumbent, unsigned int v);
unsigned int get_global_incumbent(atomic_incumbent *global_incumbent);

/* algorithm logic functions */
void add_bidomain(bidomain_list_t *bd_list, int left_i, int right_i, int left_len, int right_len, bool is_adjacent);
int calc_bound(bidomain_list_t *domains) ;
void set_incumbent(vtx_pair_list_t *current, vtx_pair_list_t *incumbent, bool verbose);
void remove_bidomain(bidomain_list_t *list, int idx);
int find_min_value(int *arr, int start_idx, int len);
void remove_vtx_from_left_domain(int *left, bidomain_t *bd, int v);
int index_of_next_smallest(int *arr, int start, int len, int w);
int select_bidomain(bidomain_list_t *domains, int *left, int current_matching_size, bool connected);
bidomain_list_t *filter_domains(bidomain_list_t *domains, int* left, int* right, graph_t *g0, graph_t *g1, int v, int w);

/* utility and wrappers */
bool check_sol(graph_t *g0, graph_t *g1 , vtx_pair_list_t *solution);

args_t *wrap_args(int depth, graph_t *g0, graph_t *g1, vtx_pair_list_t **per_thread_incumbents,
		vtx_pair_list_t *current, bidomain_list_t *domains, int *left, int *right, atomic_uint *global_incumbent,// @suppress("Type cannot be resolved")
		position_t pos, struct help_me_s *help_me, int thread_idx, atomic_uint *shared_i, // @suppress("Type cannot be resolved")
		int i_end ,int  bd_idx, bidomain_t *bd, int which_i_should_i_run_next);

args_t *copy_wrap_args(int depth, graph_t *g0, graph_t *g1, vtx_pair_list_t **per_thread_incumbents,
		vtx_pair_list_t *current, bidomain_list_t *domains, int *left, int *right, atomic_uint *global_incumbent,// @suppress("Type cannot be resolved")
		position_t pos, struct help_me_s *help_me, int thread_idx, atomic_uint  *shared_i, // @suppress("Type cannot be resolved")
		int i_end ,int  bd_idx);

vtx_pair_list_t* copy_solution(vtx_pair_list_t *src);
bidomain_list_t* copy_domains(bidomain_list_t *src);
int *copy_array(int *src, int size);

void free_domains(bidomain_list_t *old);

void free_solution(vtx_pair_list_t *old);













#endif /* UTILS_H_ */
