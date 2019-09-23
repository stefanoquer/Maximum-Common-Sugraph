/*
 * utils.c
 *
 *  Created on: Oct 12, 2018
 *      Author: gab
 */

#include "utils.h"


void swap(int *a, int *b) {
	int tmp = *a;
	*a = *b;
	*b = tmp;
}
void init_global_incumbent(atomic_incumbent *global_incumbent){
	atomic_store(global_incumbent, 0); // @suppress("Type cannot be resolved")
}

void update_global_incumbent(atomic_incumbent *global_incumbent, unsigned int v){
	while (1) {
		unsigned int cur_v = atomic_load(global_incumbent); // @suppress("Type cannot be resolved")
		if (v > cur_v) {
			if (atomic_compare_exchange_strong(global_incumbent, &cur_v, v)) return; // @suppress("Type cannot be resolved")
		} else return;
	}
}

unsigned int get_global_incumbent(atomic_incumbent *global_incumbent){
	return atomic_load(global_incumbent); // @suppress("Type cannot be resolved")
}


void add_bidomain(bidomain_list_t *bd_list, int left_i, int right_i, int left_len, int right_len, bool is_adjacent){
	bd_list->vals[bd_list->len++] = (bidomain_t) {
		.l=left_i,
				.r=right_i,
				.left_len=left_len,
				.right_len=right_len,
				.is_adjacent=is_adjacent
	};
}

int calc_bound(bidomain_list_t *domains) {
	int bound = 0;
	for(int i = 0; i < domains->len; i++){
		bound+=MIN(domains->vals[i].left_len, domains->vals[i].right_len);
	}
	return bound;
}


void set_incumbent(vtx_pair_list_t *current, vtx_pair_list_t *incumbent, bool verbose){
	incumbent->len = current->len;
	if(verbose) printf("new Incumbent: ");
	for (int i=0; i<current->len; i++){
		incumbent->vals[i] = current->vals[i];
		if(verbose) printf("|%d %d| ", incumbent->vals[i].v, incumbent->vals[i].w);
	}
	if(verbose)printf("\n");
}

void remove_bidomain(bidomain_list_t *domains, int idx){
	domains->vals[idx] = domains->vals[domains->len - 1];
	domains->len--;
}

int find_min_value(int *arr, int start_idx, int len){
	int min_v = INT_MAX;
	for(int i = 0; i < len; i++){
		if(arr[start_idx+i] < min_v)
			min_v = arr[start_idx + i];
	}
	return min_v;
}

void remove_vtx_from_left_domain(int *left, bidomain_t *bd, int v){
	int i = 0;
	while(left[bd->l + i] != v) i++;
	swap(&left[bd->l + i], &left[bd->l + bd->left_len-1]);
	bd->left_len--;
}

int index_of_next_smallest(int *arr, int start_idx, int len, int w){
	int idx = -1;
	int smallest = INT_MAX;
	for (int i=0; i<len; i++) {
		if (arr[start_idx + i]>w && arr[start_idx + i]<smallest) {
			smallest = arr[start_idx + i];
			idx = i;
		}
	}
	return idx;
}

int select_bidomain(bidomain_list_t *domains, int *left, int current_matching_size, bool connected){
	int min_size = INT_MAX;
	int min_tie_breaker = INT_MAX;
	int best = -1;
	for (unsigned int i=0; i<domains->len; i++) {
		bidomain_t *bd = &domains->vals[i];
		if (connected && current_matching_size>0 && !bd->is_adjacent) continue;
		int len = MAX(bd->left_len, bd->right_len);
		if (len < min_size) {
			min_size = len;
			min_tie_breaker = find_min_value(left, bd->l, bd->left_len);
			best = i;
		} else if (len == min_size) {
			int tie_breaker = find_min_value(left, bd->l, bd->left_len);
			if (tie_breaker < min_tie_breaker) {
				min_tie_breaker = tie_breaker;
				best = i;
			}
		}
	}
	return best;
}

int partition(int *all_vv, int start, int len, unsigned char *adjrow) {
	int i=0;
	for (int j=0; j<len; j++) {
		if (adjrow[all_vv[start+j]]) {
			swap(&all_vv[start+i], &all_vv[start+j]);
			i++;
		}
	}
	return i;
}

bidomain_list_t *filter_domains(bidomain_list_t *domains, int* left, int* right, graph_t *g0, graph_t *g1, int v, int w){

	bidomain_list_t *new_d = malloc(sizeof *new_d);
	new_d->len = 0;
	new_d->size = domains->size;
	new_d->vals = malloc(new_d->size *sizeof *new_d->vals);
	for (unsigned int i=0; i<domains->len; i++) {
		bidomain_t *old_bd = &domains->vals[i];
		int l = old_bd->l;
		int r = old_bd->r;
		// After these two partitions, left_len and right_len are the lengths of the
		// arrays of vertices with edges from v or w (int the directed case, edges
		// either from or to v or w)
		int left_len = partition(left, l, old_bd->left_len, g0->adjmat[v]);
		int right_len = partition(right, r, old_bd->right_len, g1->adjmat[w]);
		int left_len_noedge = old_bd->left_len - left_len;
		int right_len_noedge = old_bd->right_len - right_len;
		if (left_len_noedge && right_len_noedge)
			add_bidomain(new_d, l+left_len, r+right_len, left_len_noedge, right_len_noedge, old_bd->is_adjacent);
		if (left_len && right_len)
			add_bidomain(new_d, l, r, left_len, right_len, true);
	}
	return new_d;
}

bool check_sol(graph_t *g0, graph_t *g1 , vtx_pair_list_t *solution) {
	bool *used_left = calloc(g0->n, sizeof *used_left);
	bool *used_right = calloc(g1->n, sizeof *used_right);
	for (int i=0; i<solution->len; i++) {
		vtx_pair_t *p0 = &solution->vals[i];
		if (used_left[p0->v] || used_right[p0->w])
			return false;
		used_left[p0->v] = true;
		used_right[p0->w] = true;
		if (g0->label[p0->v] != g1->label[p0->w])
			return false;
		for (unsigned int j=i+1; j<solution->len; j++) {
			vtx_pair_t *p1 = &solution->vals[j];
			if (g0->adjmat[p0->v][p1->v] != g1->adjmat[p0->w][p1->w])
				return false;
		}
	}
	return true;
}



args_t *wrap_args(int depth, graph_t *g0, graph_t *g1, vtx_pair_list_t **per_thread_incumbents,
		vtx_pair_list_t *current, bidomain_list_t *domains, int *left, int *right, atomic_uint *global_incumbent,// @suppress("Type cannot be resolved")
		position_t pos, struct help_me_s *help_me, int thread_idx, atomic_uint  *shared_i, // @suppress("Type cannot be resolved")
		int i_end ,int  bd_idx, bidomain_t *bd, int which_i_should_i_run_next){

	args_t *args = malloc(sizeof *args);

	args->depth = depth;
	args->g0 = g0;
	args->g1 = g1;
	args->per_thread_incumbents = per_thread_incumbents;
	args->global_incumbent = global_incumbent;
	args->pos = pos;
	args->help_me = help_me;
	args->thread_idx = thread_idx;
	args->shared_i = shared_i;
	args->i_end = i_end;
	args->bd_idx = bd_idx;
	args->bd = bd;
	args->next_i = which_i_should_i_run_next;
	/* shallow copy */
	args->current = current;
	args->domains = domains;
	args->left = left;
	args->right = right;
	return args;

}

args_t *copy_wrap_args(int depth, graph_t *g0, graph_t *g1, vtx_pair_list_t **per_thread_incumbents,
		vtx_pair_list_t *current, bidomain_list_t *domains, int *left, int *right, atomic_uint *global_incumbent,// @suppress("Type cannot be resolved")
		position_t pos, struct help_me_s *help_me, int thread_idx, atomic_uint  *shared_i, // @suppress("Type cannot be resolved")
		int i_end ,int  bd_idx){
	args_t *args = malloc(sizeof *args);
	args->depth = depth;
	args->g0 = g0;
	args->g1 = g1;
	args->per_thread_incumbents = per_thread_incumbents;
	args->global_incumbent = global_incumbent;
	args->pos = pos;
	args->help_me = help_me;
	args->thread_idx = thread_idx;
	args->shared_i = shared_i;
	args->i_end = i_end;
	args->bd_idx = bd_idx;
	/* deep copy */
	args->current = copy_solution(current);
	args->domains = copy_domains(domains);
	args->left = copy_array(left, g0->n);
	args->right = copy_array(right, g1->n);
	return args;
}

vtx_pair_list_t* copy_solution(vtx_pair_list_t *src){
	vtx_pair_list_t * dst = malloc(sizeof *dst);
	dst->len = src->len;
	dst->size = src->size;
	dst->vals = malloc(dst->size * sizeof *dst->vals);
	memcpy(dst->vals, src->vals, dst->size * sizeof *dst->vals);
	return dst;
}

bidomain_list_t* copy_domains(bidomain_list_t *src){
	bidomain_list_t * dst = malloc(sizeof *dst);
	dst->len = src->len;
	dst->size = src->size;
	dst->vals = malloc(dst->size * sizeof *dst->vals);
	memcpy(dst->vals, src->vals, dst->size * sizeof *dst->vals);
	return dst;
}

int *copy_array(int *src, int size){
	int *dst = malloc(size * sizeof *dst);
	memcpy(dst, src, size * sizeof *dst);
	return dst;
}


void free_domains(bidomain_list_t *old){
	free(old->vals);
	free(old);
}

void free_solution(vtx_pair_list_t *old){
	free(old->vals);
	free(old);
}































