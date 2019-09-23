/*
 * main.c
 *
 *  Created on: Oct 12, 2018
 *      Author: gab
 */

#define _GNU_SOURCE
#define _POSIX_SOURCE

#include "threadpool.h"

#define N_THREAD 8

int cmp(const void *a, const void *b){
	return ((vtx_pair_t*)a)->v - ((vtx_pair_t*)b)->v;
}

static char args_doc[] = "FILENAME1 FILENAME2";
static struct argp_option options[] = {
		{"threads_num", 'n', "num", 0, "Specify number of threads"},
		{"connected", 'c', 0, 0, "Search only for connected subgraphs"},
		{"timeout", 't', "TIMEOUT", 0, "Set timeout of TIMEOUT milliseconds"},
		{"lad", 'l', 0, 0, "Read LAD format"},
		{"quiet", 'q', 0, 0, "Quiet output"},
		{"verbose", 'v', 0, 0, "Verbose output"},
		{ 0 }
};

static struct {
	int n_threads;
	bool quiet;
	bool verbose;
	bool lad;
	int timeout;
	bool connected;
	char *filename1;
	char *filename2;
	int arg_num;
} arguments;

void set_default_arguments() {
	arguments.n_threads = N_THREAD;
	arguments.quiet = false;
	arguments.verbose = false;
	arguments.lad = false;
	arguments.timeout = 0;
	arguments.connected = false;
	arguments.filename1 = NULL;
	arguments.filename2 = NULL;
	arguments.arg_num = 0;
}

error_t parse_opt (int key, char *arg, struct argp_state *state) {
	switch (key) {
	case 'n':
		arguments.n_threads = (int)strtol(arg, NULL, 10);
		break;
	case 'c':
		arguments.connected = true;
		break;
	case 'l':
	    arguments.lad = true;
	    break;
	case 'q':
		arguments.quiet = true;
		break;
	case 't':
        arguments.timeout = strtol(arg, NULL, 10);
        break;
	case 'v':
		arguments.verbose = true;
		break;
	case ARGP_KEY_ARG:
		if (arguments.arg_num == 0) {
			arguments.filename1 = arg;
		} else if (arguments.arg_num == 1) {
			arguments.filename2 = arg;
		} else {
			argp_usage(state);
		}
		arguments.arg_num++;
		break;
	case ARGP_KEY_END:
		if (arguments.arg_num == 0)
			argp_usage(state);
		break;
	default: return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc};

// ****************************************************************************************************************************
// ****************************************************************************************************************************
// ****************************************************************************************************************************
struct timespec start;

void solve (const unsigned int depth, graph_t *g0, graph_t *g1, atomic_incumbent *global_incumbent,
		vtx_pair_list_t **per_thread_incumbents, vtx_pair_list_t *current, bidomain_list_t *domains,
		int* left, int *right, position_t position, threadpool_t *help_me, int thread_idx);

double compute_elapsed_millisec(){
	struct timespec now;
	double time_elapsed;
	
	clock_gettime(CLOCK_MONOTONIC, &now);
	time_elapsed = (now.tv_sec - start.tv_sec);
	time_elapsed += (double)(now.tv_nsec - start.tv_nsec) / 1000000000.0;
	
	return time_elapsed;
}

void solve_nopar(int depth, graph_t *g0, graph_t *g1, atomic_incumbent *global_incumbent,
		vtx_pair_list_t *my_incumbent, vtx_pair_list_t *current, bidomain_list_t *domains,
		int*left, int*right, int thread_idx /* just for debug prints */){

	if (arguments.timeout && compute_elapsed_millisec() > arguments.timeout) {
        arguments.timeout = -1;
    }
    if (arguments.timeout == -1) return;
    

	if(my_incumbent->len < current->len){
		set_incumbent(current, my_incumbent, arguments.verbose);
		update_global_incumbent(global_incumbent, current->len);
	}

	if (current->len + calc_bound(domains) <= get_global_incumbent(global_incumbent))
		return;

	int bd_idx = select_bidomain(domains, left, current->len, arguments.connected);
	if(bd_idx == -1) return;

	bidomain_t *bd = &domains->vals[bd_idx];

	bd->right_len--;

	int v = find_min_value(left, bd->l, bd->left_len);
	remove_vtx_from_left_domain(left, &domains->vals[bd_idx], v);
	int w = -1;

	const int i_end = bd->right_len +2; /* including the null */

	for(int i = 0; i < i_end; i++){
		if(i != i_end - 1){
			/* try to match vertex v */
			int idx = index_of_next_smallest(right, bd->r, bd->right_len + 1, w);
			w = right[bd->r + idx];

			right[bd->r + idx] = right[bd->r + bd->right_len];
			right[bd->r + bd->right_len] = w;

			bidomain_list_t *new_domains = filter_domains(domains, left, right, g0, g1, v, w);
			current->vals[current->len++] = (vtx_pair_t){.v=v, .w=w};
			solve_nopar(depth+1, g0,g1, global_incumbent, my_incumbent, current, new_domains, left, right, thread_idx);
			free_domains(new_domains);
			current->len--;
			//FREE NEW DOMAINS
		} else {
			/* try to leave unmatched vertex v */
			bd->right_len++;
			if (bd->left_len == 0)
				remove_bidomain(domains, bd_idx);
			solve_nopar(depth + 1, g0, g1, global_incumbent, my_incumbent, current, domains, left, right, thread_idx);
		}
	}
}

void main_function(args_t *args){
	int v = find_min_value(args->left, args->bd->l, args->bd->left_len);
	remove_vtx_from_left_domain(args->left, &args->domains->vals[args->bd_idx], v);
	int w = -1;

	for (int i = 0 ; i < args->i_end /* not != */ ; i++) {
		if (i != args->i_end - 1) {
			//			for(int r = 0; r < args->g0->n; r++) printf("%d ", args->right[r]);
			//			printf("\n");
			int idx = index_of_next_smallest(args->right, args->bd->r, args->bd->right_len+1, w);
			w = args->right[args->bd->r + idx];

			// swap w to the end of its colour class
			args->right[args->bd->r + idx] = args->right[args->bd->r + args->bd->right_len];
			args->right[args->bd->r + args->bd->right_len] = w;

			if (i == args->next_i) {
				args->next_i = atomic_fetch_add(args->shared_i, 1);
				bidomain_list_t *new_domains= filter_domains(args->domains, args->left, args->right, args->g0, args->g1, v, w);
				args->current->vals[args->current->len++] = (vtx_pair_t){.v=v, .w=w};
				if (args->depth > SPLIT_LEVEL) {
					solve_nopar(args->depth + 1, args->g0, args->g1, args->global_incumbent, args->per_thread_incumbents[args->thread_idx], args->current, new_domains, args->left, args->right, args->thread_idx);
				}
				else {
					position_t new_position =args->pos;
					new_position.depth = args->depth;
					new_position.vals[new_position.depth]=  i + 1;
					solve(args->depth + 1, args->g0, args->g1, args->global_incumbent, args->per_thread_incumbents, args->current, new_domains, args->left, args->right, new_position, args->help_me, args->thread_idx);
				}
				free_domains(new_domains);
				args->current->len--;
			}
		}
		else {
			// Last assign is null. Keep it in the loop to simplify parallelism.
			args->bd->right_len++;
			if (args->bd->left_len == 0)
				remove_bidomain(args->domains, args->bd_idx);

			if (i == args->next_i) {
				args->next_i = atomic_fetch_add(args->shared_i, 1);
				if (args->depth > SPLIT_LEVEL) {
					solve_nopar(args->depth + 1, args->g0, args->g1, args->global_incumbent, args->per_thread_incumbents[args->thread_idx], args->current, args->domains, args->left, args->right, args->thread_idx);
				}
				else {
					position_t new_position =args->pos;
					new_position.depth = args->depth;
					new_position.vals[new_position.depth]=  i + 1;
					solve(args->depth + 1, args->g0, args->g1, args->global_incumbent, args->per_thread_incumbents, args->current, args->domains, args->left, args->right, new_position, args->help_me, args->thread_idx);
				}
			}
		}
	}

}

void helper_function(args_t *args){
	int next_i = atomic_fetch_add(args->shared_i, 1);

	if (next_i >= args->i_end) return;

	vtx_pair_list_t *help_current = copy_solution(args->current);
	bidomain_list_t *help_domains = copy_domains(args->domains);
	int *help_left = copy_array(args->left, args->g0->n);
	int *help_right = copy_array(args->right, args->g1->n);

	/* rerun important stuff from before the loop */
	int help_bd_idx = select_bidomain(help_domains, help_left, help_current->len, arguments.connected);
	if (help_bd_idx == -1)   // In the MCCS case, there may be nothing we can branch on
		return;
	bidomain_t *help_bd = &help_domains->vals[help_bd_idx];

	int help_v = find_min_value(help_left, help_bd->l, help_bd->left_len);
	remove_vtx_from_left_domain(help_left, &help_domains->vals[help_bd_idx], help_v);

	int help_w = -1;

	for (int i = 0 ; i < args->i_end /* not != */ ; i++) {
		if (i != args->i_end - 1) {
			int idx = index_of_next_smallest(help_right, help_bd->r, help_bd->right_len+1, help_w);
			help_w = help_right[help_bd->r + idx];
			// swap w to the end of its colour class
			help_right[help_bd->r + idx] = help_right[help_bd->r + help_bd->right_len];
			help_right[help_bd->r + help_bd->right_len] = help_w;
			if (i == next_i) {
				next_i = atomic_fetch_add(args->shared_i, 1);
				bidomain_list_t *new_domains = filter_domains(help_domains, help_left, help_right, args->g0, args->g1, help_v, help_w);
				help_current->vals[help_current->len++] = (vtx_pair_t){.v=help_v, .w=help_w};
				if (args->depth > SPLIT_LEVEL) {
					solve_nopar(args->depth + 1, args->g0, args->g1, args->global_incumbent, args->per_thread_incumbents[args->thread_idx], help_current, new_domains, help_left, help_right, args->thread_idx);
				}
				else {
					position_t new_position =args->pos;
					new_position.depth = args->depth;
					new_position.vals[new_position.depth]=  i + 1;
					solve(args->depth + 1, args->g0, args->g1, args->global_incumbent, args->per_thread_incumbents, help_current, new_domains, help_left, help_right, new_position, args->help_me, args->thread_idx);
				}
				free_domains(new_domains);
				help_current->len--;
			}
		}
		else {
			// Last assign is null. Keep it in the loop to simplify parallelism.
			help_bd->right_len++;
			if (help_bd->left_len == 0)
				remove_bidomain(help_domains, help_bd_idx);

			if (i == next_i) {
				next_i = atomic_fetch_add(args->shared_i, 1);
				if (args->depth > SPLIT_LEVEL) {
					solve_nopar(args->depth + 1, args->g0, args->g1, args->global_incumbent, args->per_thread_incumbents[args->thread_idx], help_current, help_domains, help_left, help_right, args->thread_idx);
				}
				else {
					position_t new_position = args->pos;
					new_position.depth = args->depth;
					new_position.vals[new_position.depth]=  i + 1;
					solve(args->depth + 1, args->g0, args->g1, args->global_incumbent, args->per_thread_incumbents, help_current, help_domains, help_left, help_right, new_position, args->help_me, args->thread_idx);
				}
			}
		}
	}
}


void solve (const unsigned int depth, graph_t *g0, graph_t *g1, atomic_incumbent *global_incumbent,
		vtx_pair_list_t **per_thread_incumbents, vtx_pair_list_t *current, bidomain_list_t *domains,
		int* left, int *right, position_t position, threadpool_t *help_me, int thread_idx){
		
	if (arguments.timeout && compute_elapsed_millisec() > arguments.timeout) {
        arguments.timeout = -1;
    }
    if (arguments.timeout == -1) return;
    
	if(per_thread_incumbents[thread_idx]->len < current->len){
		set_incumbent(current, per_thread_incumbents[thread_idx], arguments.verbose);
		update_global_incumbent(global_incumbent, current->len);
	}

	unsigned int bound = current->len + calc_bound(domains);
	if (bound <= get_global_incumbent(global_incumbent))
		return;

	int bd_idx = select_bidomain(domains, left, current->len, arguments.connected);
	if (bd_idx == -1)   // In the MCCS case, there may be nothing we can branch on
		return;
	bidomain_t *bd = &domains->vals[bd_idx];

	bd->right_len = bd->right_len - 1;
	atomic_uint *shared_i = malloc(sizeof *shared_i); 	// @suppress("Type cannot be resolved")
	atomic_store(shared_i, 0); 								// @suppress("Type cannot be resolved")
	const int i_end = bd->right_len +2;

	int next_i = atomic_fetch_add(shared_i, 1);

	args_t *main_args = wrap_args(depth, g0, g1, per_thread_incumbents,
			current, domains, left, right, global_incumbent,// @suppress("Type cannot be resolved")
			position, help_me, thread_idx, shared_i, // @suppress("Type cannot be resolved")
			i_end , bd_idx, bd, next_i);;

	if (depth <= SPLIT_LEVEL){
		args_t *helper_args = copy_wrap_args(depth, g0, g1, per_thread_incumbents,
				current, domains, left, right, global_incumbent,// @suppress("Type cannot be resolved")
				position, help_me, thread_idx, shared_i, // @suppress("Type cannot be resolved")
				i_end, bd_idx);
		get_help_with(position, help_me, main_function, helper_function, main_args, helper_args);
	} else {
		main_function(main_args);
	}
	free(shared_i);
	free(main_args);
}

vtx_pair_list_t *mcs(graph_t *g0, graph_t *g1){

	int size = MIN(g0->n, g1->n);

	vtx_pair_list_t *current = calloc(1, sizeof *current);
	current->size = size;
	current->vals = calloc(current->size, sizeof *current->vals);

	bidomain_list_t *domains = calloc(1, sizeof *domains);
	domains->size = size;
	domains->vals = calloc(current->size, sizeof *domains->vals);

	vtx_pair_list_t **per_thread_incumbents = malloc(N_THREAD*sizeof *per_thread_incumbents);
	for(int i = 0; i < N_THREAD; i++) {
		per_thread_incumbents[i] = calloc(1, sizeof *per_thread_incumbents[i]);
		per_thread_incumbents[i]->size = size;
		per_thread_incumbents[i]->vals = calloc(per_thread_incumbents[i]->size, sizeof *per_thread_incumbents[i]->vals);;
	}

	atomic_incumbent incumbent;

	init_global_incumbent(&incumbent);

	int *left = malloc(g0->n* sizeof *left );  // the buffer of vertex indices for the left partitions
	int *right = malloc(g1->n* sizeof *right );  // the buffer of vertex indices for the right partitions
	int l = 0, r = 0;  // next free index in left and right

	// Create a bidomain for vertices without loops (label 0),
	// and another for vertices with loops (label 1)
	for (int label=0; label<=1; label++) {
		int start_l = l;
		int start_r = r;
		for (int i=0; i<g0->n; i++)
			if (g0->label[i]==label)
				left[l++] = i;
		for (int i=0; i<g1->n; i++)
			if (g1->label[i]==label)
				right[r++] = i;

		int left_len = l - start_l;
		int right_len = r - start_r;
		if (left_len && right_len)
			add_bidomain(domains, start_l, start_r, left_len, right_len, false);
	}


	threadpool_t *help_me = init_threadpool(N_THREAD-1);
	solve (0, g0, g1, &incumbent, per_thread_incumbents, current, domains, left, right, (position_t){.depth=0, .vals={0,0,0,0,0}}, help_me, 0);
	kill_workers(help_me);

	vtx_pair_list_t *solution= NULL;
	for(int i = 0; i < N_THREAD; i++){
		if(per_thread_incumbents[i]->len == get_global_incumbent(&incumbent)){
			qsort(per_thread_incumbents[i]->vals, per_thread_incumbents[i]->len, sizeof(vtx_pair_t), cmp );
			solution = copy_solution(per_thread_incumbents[i]);
		}
		free(per_thread_incumbents[i]->vals);
		free(per_thread_incumbents[i]);
	}
	free(per_thread_incumbents);
	free(left);
	free(right);
	free_domains(domains);
	free(current);
	return solution;

}

int main(int argc, char** argv){
	set_default_arguments();
	argp_parse(&argp, argc, argv, 0, 0, 0);

	struct timespec finish;
	double time_elapsed;

	char format = arguments.lad ? 'L' : 'B';
	graph_t *g0 = calloc(1, sizeof *g0 );
	readGraph(arguments.filename1, g0, format);
	graph_t *g1 = calloc(1, sizeof *g1 );
	readGraph(arguments.filename2, g1, format);
	g0 = sort_vertices_by_degree(g0, (graph_edge_count(g1) > g1->n*(g1->n-1)/2));
	g1 = sort_vertices_by_degree(g1, (graph_edge_count(g0) > g0->n*(g0->n-1)/2));



	clock_gettime(CLOCK_MONOTONIC, &start);
	vtx_pair_list_t *solution = mcs(g0, g1);
	clock_gettime(CLOCK_MONOTONIC, &finish);

	if (!check_sol(g0, g1, solution)) {
		fprintf(stderr, "*** Error: Invalid solution\n");
	} else {
	
		if (arguments.timeout == -1){
        	printf("TIMEOUT\n");
		}
	
		for(int j = 0; j < solution->len; j++)
			printf("(%d - %d) ", solution->vals[j].v, solution->vals[j].w);
		printf("\n");
		time_elapsed = (finish.tv_sec - start.tv_sec);
		time_elapsed += (double)(finish.tv_nsec - start.tv_nsec) / 1000000000.0;
		printf(">>> %d - %015.10f\n", solution->len, time_elapsed);
	}
	free_solution(solution);
	free_graph(g0);
	free_graph(g1);
	return 0;
}
