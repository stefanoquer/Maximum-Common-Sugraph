#include <argp.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

#define START 0
#define END 1

#define MIN(a, b) (a < b)? a : b

#define N_BLOCKS 64
#define BLOCK_SIZE 512
#define MAX_GRAPH_SIZE 64
#define checkCudaErrors(value) CheckCudaErrorAux(__FILE__,__LINE__, #value, value)

typedef unsigned char uchar;
typedef unsigned int uint;

__constant__ uchar d_adjmat0[MAX_GRAPH_SIZE][MAX_GRAPH_SIZE];
__constant__ uchar d_adjmat1[MAX_GRAPH_SIZE][MAX_GRAPH_SIZE];
__constant__ uchar d_n0;
__constant__ uchar d_n1;

uchar adjmat0[MAX_GRAPH_SIZE][MAX_GRAPH_SIZE];
uchar adjmat1[MAX_GRAPH_SIZE][MAX_GRAPH_SIZE];
uchar n0;
uchar n1;

uint __gpu_level = 5;
struct timespec start;

static struct argp_option options[] = {
		{ "verbose", 'v', 0, 0, "Verbose output" },
		{ "lad", 'l', 0, 0, "Read LAD format"},
		{ "timeout", 't', "timeout", 0, "Set timeout of TIMEOUT milliseconds"},
		{ "connected", 'c', 0, 0, "Solve max common CONNECTED subgraph problem" },
		{ 0 }
};

static char doc[] = "Find a maximum isomorphic graph";
static char args_doc[] = "FILENAME1 FILENAME2";
static struct {
	bool verbose;
	bool lad;
	bool connected;
	int timeout;
	char *filename1;
	char *filename2;
	int arg_num;
} arguments;
void set_default_arguments() {
	arguments.verbose = false;
	arguments.lad = false;
	arguments.timeout = 0;
	arguments.connected = false;
	arguments.filename1 = NULL;
	arguments.filename2 = NULL;
	arguments.arg_num = 0;
}
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	switch (key) {
	case 'v':
		arguments.verbose = true;
		break;
	case 't':
		arguments.timeout = strtol(arg, NULL, 10);
		break;
	case 'l':
		arguments.lad = true;
		break;
	case 'c':
		arguments.connected = true;
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
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

__host__ __device__
void uchar_swap(uchar *a, uchar *b){
	uchar tmp = *a;
	*a = *b;
	*b = tmp;
}

__host__ __device__
uchar select_next_v(uchar *left, uchar *bd){
	uchar min = UCHAR_MAX, idx = UCHAR_MAX;
	if(bd[RL] != bd[IRL])
		return left[bd[L] + bd[LL]];
	for (uchar i = 0; i < bd[LL]; i++)
		if (left[bd[L] + i] < min) {
			min = left[bd[L] + i];
			idx = i;
		}
	uchar_swap(&left[bd[L] + idx], &left[bd[L] + bd[LL] - 1]);
	bd[LL]--;
	bd[RL]--;
	return min;
}


__host__ __device__
uchar select_next_w(uchar *right, uchar *bd) {
	uchar min = UCHAR_MAX, idx = UCHAR_MAX;
	for (uchar i = 0; i < bd[RL]+1; i++)
		if ((right[bd[R] + i] > bd[W] || bd[W] == UCHAR_MAX)
				&& right[bd[R] + i] < min) {
			min = right[bd[R] + i];
			idx = i;
		}
	if(idx == UCHAR_MAX)
		bd[RL]++;
	return idx;
}


__host__  __device__ uchar index_of_next_smallest(const uchar *arr,
		uchar start_idx, uchar len, uchar w) {
	uchar idx = UCHAR_MAX;
	uchar smallest = UCHAR_MAX;
	for (uchar i = 0; i < len; i++) {
		if ((arr[start_idx + i] > w || w == UCHAR_MAX)
				&& arr[start_idx + i] < smallest) {
			smallest = arr[start_idx + i];
			idx = i;
		}
	}
	return idx;
}

__host__  __device__ uchar find_min_value(const uchar *arr, uchar start_idx,
		uchar len) {
	uchar min_v = UCHAR_MAX;
	for (int i = 0; i < len; i++) {
		if (arr[start_idx + i] < min_v)
			min_v = arr[start_idx + i];
	}
	return min_v;
}

__host__ __device__
void remove_from_domain(uchar *arr, const uchar *start_idx, uchar *len,
		uchar v) {
	int i = 0;
	for (i = 0; arr[*start_idx + i] != v; i++)
		;
	uchar_swap(&arr[*start_idx + i], &arr[*start_idx + *len - 1]);
	(*len)--;
}

__host__ __device__
void update_incumbent(uchar cur[][2], uchar inc[][2], uchar cur_pos,
		uchar *inc_pos) {
	if (cur_pos > *inc_pos) {
		*inc_pos = cur_pos;
		for (int i = 0; i < cur_pos; i++) {
			inc[i][L] = cur[i][L];
			inc[i][R] = cur[i][R];
		}
	}
}

// BIDOMAINS FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////////
__host__ __device__
void add_bidomain(uchar domains[][BDS], uint *bd_pos, uchar left_i,
		uchar right_i, uchar left_len, uchar right_len, uchar is_adjacent,
		uchar cur_pos) {
	domains[*bd_pos][L] 	= left_i;
	domains[*bd_pos][R] 	= right_i;
	domains[*bd_pos][LL] 	= left_len;
	domains[*bd_pos][RL] 	= right_len;
	domains[*bd_pos][ADJ] 	= is_adjacent;
	domains[*bd_pos][P] 	= cur_pos;
	domains[*bd_pos][W] 	= UCHAR_MAX;
	domains[*bd_pos][IRL] 	= right_len;

	(*bd_pos)++;
}

__host__  __device__ uint calc_bound(uchar domains[][BDS], uint bd_pos,
		uint cur_pos, uint *bd_n) {
	uint bound = 0;
	int i;
	for (i = bd_pos - 1; i >= 0 && domains[i][P] == cur_pos; i--)
		bound += MIN(domains[i][LL], domains[i][IRL]);
	*bd_n = bd_pos - 1 - i;
	return bound;
}

__host__  __device__ uchar partition(uchar *arr, uchar start, uchar len,
		const uchar *adjrow) {
	uchar i = 0;
	for (uchar j = 0; j < len; j++) {
		if (adjrow[arr[start + j]]) {
			uchar_swap(&arr[start + i], &arr[start + j]);
			i++;
		}
	}
	return i;
}

__host__  __device__
uchar find_min_value(uchar *arr, uchar start_idx, uchar len){
	uchar min_v = UCHAR_MAX;
	for(int i = 0; i < len; i++){
		if(arr[start_idx+i] < min_v)
			min_v = arr[start_idx + i];
	}
	return min_v;
}

__host__  __device__
void select_bidomain(uchar domains[][BDS], uint bd_pos,  uchar *left, int current_matching_size, bool connected){
	int i;
	uint min_size = UINT_MAX;
	uint min_tie_breaker = UINT_MAX;
	uint best = UINT_MAX;
	uchar *bd;
	for (i = bd_pos - 1, bd = &domains[i][L]; i >= 0 && bd[P] == current_matching_size; i--, bd = &domains[i][L]) {
		if (connected && current_matching_size>0 && !bd[ADJ]) continue;
		int len = bd[LL] > bd[RL] ? bd[LL] : bd[RL];
		if (len < min_size) {
			min_size = len;
			min_tie_breaker = find_min_value(left, bd[L], bd[LL]);
			best = i;
		} else if (len == min_size) {
			int tie_breaker = find_min_value(left, bd[L], bd[LL]);
			if (tie_breaker < min_tie_breaker) {
				min_tie_breaker = tie_breaker;
				best = i;
			}
		}
	}
	if(best != UINT_MAX && best != bd_pos-1){
		uchar tmp[BDS];
		for(i = 0; i < BDS; i++) tmp[i] = domains[best][i];
		for(i = 0; i < BDS; i++) domains[best][i] = domains[bd_pos-1][i];
		for(i = 0; i < BDS; i++) domains[bd_pos-1][i] = tmp[i];

	}
}



__device__
void d_generate_next_domains(uchar domains[][BDS], uint *bd_pos, uint cur_pos, uchar *left, uchar *right, uchar v, uchar w, uint inc_pos) {
	int i;
	uint bd_backup = *bd_pos;
	uint bound = 0;
	uchar *bd;
	for (i = *bd_pos - 1, bd = &domains[i][L]; i >= 0 && bd[P] == cur_pos - 1; i--, bd = &domains[i][L]) {

		uchar l_len = partition(left, bd[L], bd[LL], d_adjmat0[v]);
		uchar r_len = partition(right, bd[R], bd[RL], d_adjmat1[w]);

		if (bd[LL] - l_len && bd[RL] - r_len) {
			add_bidomain(domains, bd_pos, bd[L] + l_len, bd[R] + r_len, bd[LL] - l_len, bd[RL] - r_len, bd[ADJ], (uchar) (cur_pos));
			bound += MIN(bd[LL] - l_len, bd[RL] - r_len);
		}
		if (l_len && r_len) {
			add_bidomain(domains, bd_pos, bd[L], bd[R], l_len, r_len, true, (uchar) (cur_pos));
			bound += MIN(l_len, r_len);
		}
	}
	if (cur_pos + bound <= inc_pos)
		*bd_pos = bd_backup;
}

__global__
void d_mcs(uchar *args, uint n_threads, uchar a_size, uint *args_i, uint actual_inc, uchar *device_solutions, uint max_sol_size, uint last_arg, bool verbose, bool connected) {
	uint my_idx = (blockIdx.x * blockDim.x) + threadIdx.x;
	uchar cur[MAX_GRAPH_SIZE][2], incumbent[MAX_GRAPH_SIZE][2],
	domains[MAX_GRAPH_SIZE * 5][BDS], left[MAX_GRAPH_SIZE],
	right[MAX_GRAPH_SIZE], v, w;
	uint bd_pos = 0, bd_n = 0;
	uchar inc_pos = 0;
	__shared__ uint sh_inc;
	sh_inc = actual_inc;
	__syncthreads();
	if (my_idx < n_threads) {
		for (int i = args_i[my_idx]; i < last_arg && ( my_idx < n_threads-1 &&  i < args_i[my_idx +1]);) {
			add_bidomain(domains, &bd_pos, args[i++], args[i++], args[i++], args[i++], args[i++], args[i++]);
			for (int p = 0; p < domains[bd_pos - 1][P]; p++)
				cur[p][L] = args[i++];
			for (int p = 0; p < domains[bd_pos - 1][P]; p++)
				cur[p][R] = args[i++];
			for (int l = 0; l < d_n0; l++)
				left[l] = args[i++];
			for (int r = 0; r < d_n1; r++)
				right[r] = args[i++];
		}
		while (bd_pos > 0) {
			uchar *bd = &domains[bd_pos - 1][L];
			if (calc_bound(domains, bd_pos, bd[P], &bd_n) + bd[P] <= sh_inc	|| (bd[LL] == 0 && bd[RL] == bd[IRL])) {
				bd_pos--;
			} else {
				select_bidomain(domains, bd_pos, left, domains[bd_pos - 1][P], connected);
				if (bd[RL] == bd[IRL]) {
					v = find_min_value(left, bd[L], bd[LL]);
					remove_from_domain(left, &bd[L], &bd[LL], v);
					bd[RL]--;
				} else v = left[bd[L] + bd[LL]];
				if ((bd[W] = index_of_next_smallest(right, bd[R], bd[RL] + (uchar) 1, bd[W])) == UCHAR_MAX) {
					bd[RL]++;
				} else {
					w = right[bd[R] + bd[W]];
					right[bd[R] + bd[W]] = right[bd[R] + bd[RL]];
					right[bd[R] + bd[RL]] = w;
					bd[W] = w;
					cur[bd[P]][L] = v;
					cur[bd[P]][R] = w;
					update_incumbent(cur, incumbent, bd[P] + 1, &inc_pos);
					atomicMax(&sh_inc, inc_pos);
					d_generate_next_domains(domains, &bd_pos, bd[P] + 1, left, right, v, w, inc_pos);
				}
			}
		}
	}
	device_solutions[blockIdx.x* max_sol_size] = 0;

	__syncthreads();
	if (atomicCAS(&sh_inc, inc_pos, 0) == inc_pos && inc_pos > 0) {
		if(verbose) printf("Th_%d found new solution of size %d\n", my_idx, inc_pos);
		bd_pos = 0;
		device_solutions[blockIdx.x* max_sol_size + bd_pos++] = inc_pos;
		for (int i = 0; i < inc_pos; i++)
			device_solutions[blockIdx.x* max_sol_size + bd_pos++] = incumbent[i][L];
		for (int i = 0; i < inc_pos; i++)
			device_solutions[blockIdx.x* max_sol_size + bd_pos++] = incumbent[i][R];
	}
}

double compute_elapsed_sec(struct timespec strt){
	struct timespec now;
	double time_elapsed;

	clock_gettime(CLOCK_MONOTONIC, &now);
	time_elapsed = (now.tv_sec - strt.tv_sec);
	time_elapsed += (double)(now.tv_nsec - strt.tv_nsec) / 1000000000.0;

	return time_elapsed;
}

static void CheckCudaErrorAux(const char *file, unsigned line,
		const char *statement, cudaError_t err) {
	if (err == cudaSuccess)
		return;
	fprintf(stderr, "%s returned %s(%d) at %s:%d\n", statement,
			cudaGetErrorString(err), err, file, line);
	exit(1);
}

void move_graphs_to_gpu(graph_t *g0, graph_t *g1) {
	checkCudaErrors(cudaMemcpyToSymbol(d_n0, &g0->n, sizeof(uchar)));
	checkCudaErrors(cudaMemcpyToSymbol(d_n1, &g1->n, sizeof(uchar)));
	checkCudaErrors(cudaMemcpyToSymbol(d_adjmat0, adjmat0, MAX_GRAPH_SIZE*MAX_GRAPH_SIZE));
	checkCudaErrors(cudaMemcpyToSymbol(d_adjmat1, adjmat1, MAX_GRAPH_SIZE*MAX_GRAPH_SIZE));
}


void h_generate_next_domains(uchar domains[][BDS], uint *bd_pos, uint cur_pos,
		uchar *left, uchar *right, uchar v, uchar w, uint inc_pos) {
	int i;
	uint bd_backup = *bd_pos;
	uint bound = 0;
	uchar *bd;
	for (i = *bd_pos - 1, bd = &domains[i][L]; i >= 0 && bd[P] == cur_pos - 1;
			i--, bd = &domains[i][L]) {

		uchar l_len = partition(left, bd[L], bd[LL], adjmat0[v]);
		uchar r_len = partition(right, bd[R], bd[RL], adjmat1[w]);

		if (bd[LL] - l_len && bd[RL] - r_len) {
			add_bidomain(domains, bd_pos, bd[L] + l_len, bd[R] + r_len,
					bd[LL] - l_len, bd[RL] - r_len, bd[ADJ], (uchar) (cur_pos));
			bound += MIN(bd[LL] - l_len, bd[RL] - r_len);
		}
		if (l_len && r_len) {
			add_bidomain(domains, bd_pos, bd[L], bd[R], l_len, r_len, true,
					(uchar) (cur_pos));
			bound += MIN(l_len, r_len);
		}
	}
	if (cur_pos + bound <= inc_pos)
		*bd_pos = bd_backup;
}


bool check_sol(graph_t *g0, graph_t *g1, uchar sol[][2], uint sol_len) {
	bool *used_left = (bool*) calloc(g0->n, sizeof *used_left);
	bool *used_right = (bool*) calloc(g1->n, sizeof *used_right);
	for (int i = 0; i < sol_len; i++) {
		if (used_left[sol[i][L]]) {
			printf("node %d of g0 used twice\n", used_left[sol[i][L]]);
			return false;
		}
		if (used_right[sol[i][R]]) {
			printf("node %d of g1 used twice\n", used_right[sol[i][L]]);
			return false;
		}
		used_left[sol[i][L]] = true;
		used_right[sol[i][R]] = true;
		if (g0->label[sol[i][L]] != g1->label[sol[i][R]]) {
			printf("g0:%d and g1:%d have different labels\n", sol[i][L],
					sol[i][R]);
			return false;
		}
		for (int j = i + 1; j < sol_len; j++) {
			if (g0->adjmat[sol[i][L]][sol[j][L]]
			                          != g1->adjmat[sol[i][R]][sol[j][R]]) {
				printf("g0(%d-%d) is different than g1(%d-%d)\n", sol[i][L],
						sol[j][L], sol[i][R], sol[j][R]);
				return false;
			}
		}
	}
	return true;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

void launch_kernel(uchar *args, uint n_threads, uchar a_size, uint sol_size, uint *args_i,
		uchar incumbent[][2], uchar *inc_pos, uint total_args_size, uint last_arg) {
	uchar *device_args;
	uchar *device_solutions;
	uchar *host_solutions;
	uint *device_args_i;
	uint max_sol_size = 1 + 2 * (MIN(n0, n1));
	struct timespec sleep;
	sleep.tv_sec = 0;
	sleep.tv_nsec = 2000;
	cudaEvent_t stop;

	host_solutions = (uchar*) malloc(N_BLOCKS * max_sol_size * sizeof *host_solutions);

	checkCudaErrors(cudaEventCreate(&stop));

	checkCudaErrors(cudaMalloc(&device_args, total_args_size * sizeof *device_args));
	checkCudaErrors(cudaMalloc(&device_solutions, N_BLOCKS * max_sol_size * sizeof *device_solutions));


	checkCudaErrors(cudaMemcpy(device_args, args, total_args_size * sizeof *device_args, cudaMemcpyHostToDevice));

	checkCudaErrors(cudaMalloc(&device_args_i, N_BLOCKS * BLOCK_SIZE * sizeof *device_args_i));
	checkCudaErrors(cudaMemcpy(device_args_i, args_i, N_BLOCKS * BLOCK_SIZE * sizeof *device_args_i, cudaMemcpyHostToDevice));

	if(arguments.verbose) printf("Launching kernel...\n");

	d_mcs<<<N_BLOCKS, BLOCK_SIZE>>>(device_args, n_threads, a_size, device_args_i, *inc_pos, device_solutions, max_sol_size, last_arg, arguments.verbose, arguments.connected);
	checkCudaErrors(cudaEventRecord(stop));

	while(cudaEventQuery(stop) == cudaErrorNotReady){
		nanosleep(&sleep, NULL);
		if(arguments.timeout && compute_elapsed_sec(start) > arguments.timeout)
			return;
	}

	if(arguments.verbose) printf("Kernel executed...\n");

	checkCudaErrors(cudaMemcpy(host_solutions, device_solutions, N_BLOCKS * max_sol_size * sizeof *device_solutions, cudaMemcpyDeviceToHost));

	checkCudaErrors(cudaFree(device_args));
	checkCudaErrors(cudaFree(device_args_i));

	for(int b = 0; b < N_BLOCKS; b++){
		if (*inc_pos < host_solutions[b*max_sol_size]) {
			*inc_pos = host_solutions[b*max_sol_size];
			for (int i = 1; i < *inc_pos + 1; i++) {
				incumbent[i - 1][L] = host_solutions[b*max_sol_size + i];
				incumbent[i - 1][R] = host_solutions[b*max_sol_size + *inc_pos + i];
				if(arguments.verbose) printf("|%d %d| ", incumbent[i-1][L], incumbent[i-1][R]);
			}if(arguments.verbose) printf("\n");
		}
	}
	free(host_solutions);
}

void *safe_realloc(void* old, uint new_size){
	void *tmp = realloc(old, new_size);
	if (tmp != NULL) return tmp;
	else exit(-1);
}

void mcs(uchar incumbent[][2], uchar *inc_pos) {
	uint bd_pos = 0, bd_n = 0;
	uchar cur[MAX_GRAPH_SIZE][2], domains[MAX_GRAPH_SIZE * 5][BDS], left[n0],
	right[n1], v, w;
	for (uchar i = 0; i < n0; i++)
		left[i] = i;
	for (uchar i = 0; i < n1; i++)
		right[i] = i;
	add_bidomain(domains, &bd_pos, 0, 0, n0, n1, 0, 0);
	//supposing an initial average of 2 domains for thread, it will be reallocated if necessary
	uint args_num = N_BLOCKS * BLOCK_SIZE * 2;
	uint a_size = (BDS - 2 + 2 * __gpu_level + n0 + n1);
	uint sol_size = 1 + 2*(MIN(n0, n1));
	uint args_size = args_num * a_size;

	uint args_i[N_BLOCKS * BLOCK_SIZE];
	uchar *args = (uchar*) malloc(args_size * sizeof *args);
	uint n_args = 0, n_threads = 0;

	while (bd_pos > 0) {
		if (arguments.timeout && compute_elapsed_sec(start) > arguments.timeout) {
			arguments.timeout = -1;
			return;
		}
		uchar *bd = &domains[bd_pos - 1][L];

		if (calc_bound(domains, bd_pos, bd[P], &bd_n) + bd[P] <= *inc_pos || (bd[LL] == 0 && bd[RL] == bd[IRL])) {
			bd_pos--;
			continue;
		}

		if (bd[P] == __gpu_level) {
			if (n_args + bd_n > args_num) {
				args_num  = n_args + bd_n;
				args_size = args_num * a_size;
				args = (uchar*) safe_realloc(args, args_size * sizeof *args);
			}

			args_i[n_threads] = n_args * a_size;

			for (uint b = 0; b < bd_n; b++, n_args++, bd_pos--) {
				uint arg_i = n_args * a_size, i = 0;
				for (i = 0; i < BDS - 2; i++, arg_i++)
					args[arg_i] = domains[bd_pos - 1][i];
				for (i = 0; i < __gpu_level; i++, arg_i++)
					args[arg_i] = cur[i][L];
				for (i = 0; i < __gpu_level; i++, arg_i++)
					args[arg_i] = cur[i][R];
				for (i = 0; i < n0; i++, arg_i++)
					args[arg_i] = left[i];
				for (i = 0; i < n0; i++, arg_i++)
					args[arg_i] = right[i];
			}
			n_threads++;
			if (n_threads == N_BLOCKS * BLOCK_SIZE) {
				launch_kernel(args, n_threads, a_size, sol_size, args_i, incumbent, inc_pos, args_size, n_args*a_size);
				n_threads = 0;
				n_args = 0;
			}
			continue;
		}

		select_bidomain(domains, bd_pos, left, domains[bd_pos - 1][P], arguments.connected);
		if (bd[RL] == bd[IRL]) {
			v = find_min_value(left, bd[L], bd[LL]);
			remove_from_domain(left, &bd[L], &bd[LL], v);
			bd[RL]--;
		} else v = left[bd[L] + bd[LL]];


		if ((bd[W] = index_of_next_smallest(right, bd[R], bd[RL] + (uchar) 1, bd[W])) == UCHAR_MAX) {
			bd[RL]++;
		} else {
			w = right[bd[R] + bd[W]];
			right[bd[R] + bd[W]] = right[bd[R] + bd[RL]];
			right[bd[R] + bd[RL]] = w;

			bd[W] = w;

			cur[bd[P]][L] = v;
			cur[bd[P]][R] = w;

			update_incumbent(cur, incumbent, bd[P] + 1, inc_pos);
			h_generate_next_domains(domains, &bd_pos, bd[P] + 1, left, right, v,
					w, *inc_pos);
		}

	}
	if (n_threads > 0)
		launch_kernel(args, n_threads, a_size, sol_size, args_i, incumbent, inc_pos, args_size, n_args*a_size);
}

int main(int argc, char** argv) {
	set_default_arguments();
	argp_parse(&argp, argc, argv, 0, 0, 0);
	struct timespec finish;
	double time_elapsed;
	char format = arguments.lad ? 'L' : 'B';
	graph_t *g0 = (graph_t*) calloc(1, sizeof *g0);
	readGraph(arguments.filename1, g0, format);
	graph_t *g1 = (graph_t*) calloc(1, sizeof *g1);
	readGraph(arguments.filename2, g1, format);
	g0 = sort_vertices_by_degree(g0,
			(graph_edge_count(g1) > g1->n * (g1->n - 1) / 2));
	g1 = sort_vertices_by_degree(g1,
			(graph_edge_count(g0) > g0->n * (g0->n - 1) / 2));
	int min_size = MIN(g0->n, g1->n);
	n0 = g0->n;
	n1 = g1->n;

	for (int i = 0; i < n0; i++)
		for (int j = 0; j < n0; j++)
			adjmat0[i][j] = g0->adjmat[i][j];

	for (int i = 0; i < n1; i++)
		for (int j = 0; j < n1; j++)
			adjmat1[i][j] = g1->adjmat[i][j];
	checkCudaErrors(cudaDeviceReset());
	move_graphs_to_gpu(g0, g1);
	uchar solution[min_size][2];
	uchar sol_len = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	mcs(solution, &sol_len);
	clock_gettime(CLOCK_MONOTONIC, &finish);

	if(arguments.timeout == -1){
		printf("TIMEOUT\n");
	}

	printf("------------------------------------------------------------\n");
	printf("SOLUTION size:%d\nsol: ", sol_len);
	//for (int i = 0; i < g0->n; i++)
	for (int j = 0; j < sol_len; j++)
		//if (solution[j][L] == i)
		printf("|%2d %2d| ", solution[j][L], solution[j][R]);
	printf("\n");

	if (!check_sol(g0, g1, solution, sol_len)) {
		printf("*** Error: Invalid solution\n");
	}
	time_elapsed = (finish.tv_sec - start.tv_sec); // calculating elapsed seconds
	time_elapsed += (double) (finish.tv_nsec - start.tv_nsec) / 1000000000.0; // adding elapsed nanoseconds
	printf(">>> %d - %015.10f\n", sol_len, time_elapsed);

	free_graph(g0);
	free_graph(g1);
	return 0;
}
