#define _GNU_SOURCE
#define _POSIX_SOURCE


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

#define MIN(a, b) (a < b)? a : b

#define STACK_RESIZE 4

typedef struct stack {
	unsigned pos, size;
	int (*stack)[BDS];
}stack;

static struct argp_option options[] = {
		{"quiet", 'q', 0, 0, "Quiet output"},
		{"verbose", 'v', 0, 0, "Verbose output"},
		{"lad", 'l', 0, 0, "Read LAD format"},
		{"timeout", 't', "timeout", 0, "Set timeout of TIMEOUT milliseconds"},
		{"connected", 'c', 0, 0, "Solve max common CONNECTED subgraph problem"},
		{ 0 }
};

static char doc[] = "Find a maximum isomorphic graph";
static char args_doc[] = "FILENAME1 FILENAME2";
static struct {
	bool quiet;
	bool verbose;
	bool connected;
	bool lad;
    int timeout;
	char *filename1;
	char *filename2;
	int arg_num;
} arguments;

void set_default_arguments() {
	arguments.quiet = false;
	arguments.verbose = false;
	arguments.lad = false;
    arguments.timeout = 0;
	arguments.connected = false;
	arguments.filename1 = NULL;
	arguments.filename2 = NULL;
	arguments.arg_num = 0;
}
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
	switch (key) {
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
	default: return ARGP_ERR_UNKNOWN;
	}
	return 0;
}
static struct argp argp = { options, parse_opt, args_doc, doc };

uchar **adjmat0, **adjmat1, n0, n1;
uint max_dom = 0;
struct timespec start;

void uchar_swap(uchar *a, uchar *b){
	uchar tmp = *a;
	*a = *b;
	*b = tmp;
}

bool check_sol(graph_t *g0, graph_t *g1 , uchar sol[][2], uint sol_len) {
	bool *used_left = (bool*)calloc(g0->n, sizeof *used_left);
	bool *used_right = (bool*)calloc(g1->n, sizeof *used_right);
	for (int i = 0; i < sol_len; i++) {
		if (used_left[sol[i][L]])  {
			printf("node %d of g0 used twice\n", used_left[sol[i][L]]);
			return false;
		}
		if (used_right[sol[i][R]])  {
			printf("node %d of g1 used twice\n", used_right[sol[i][L]]);
			return false;
		}
		used_left[sol[i][L]] = true;
		used_right[sol[i][R]] = true;
		if (g0->label[sol[i][L]] != g1->label[sol[i][R]]){
			printf("g0:%d and g1:%d have different labels\n", sol[i][L], sol[i][R]);
			return false;
		}
		for (int j = i + 1; j < sol_len; j++) {
			if (g0->adjmat[sol[i][L]][sol[j][L]] != g1->adjmat[sol[i][R]][sol[j][R]])
			{
				printf("g0(%d-%d) is different than g1(%d-%d)\n",sol[i][L],sol[j][L], sol[i][R], sol[j][R]);
				return false;
			}
		}
	}
	return true;
}

void update_incumbent(uchar cur[][2], uchar inc[][2], uint cur_pos, uint *inc_pos){
	if(cur_pos > *inc_pos){
		*inc_pos = cur_pos;
		if(arguments.verbose) printf("New incumbent size: %d\n", *inc_pos);

		for(int i = 0; i < cur_pos; i++){
			inc[i][L] = cur[i][L];
			inc[i][R] = cur[i][R];
		}
	}
}

// BIDOMAINS FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////////
void add_bidomain(uchar domains[][BDS], uint *bd_pos, uchar left_i, uchar right_i, uchar left_len, uchar right_len, uchar is_adjacent, uchar cur_pos){
	domains[*bd_pos][L] = left_i;
	domains[*bd_pos][R] = right_i;
	domains[*bd_pos][LL] = left_len;
	domains[*bd_pos][RL] = right_len;
	domains[*bd_pos][ADJ] = is_adjacent;
	domains[*bd_pos][P] = cur_pos;
	domains[*bd_pos][W] = UCHAR_MAX;
	domains[*bd_pos][IRL] = right_len;
	(*bd_pos)++;
	if(*bd_pos > max_dom) max_dom = *bd_pos;
}

uint calc_bound(uchar domains[][BDS], uint bd_pos, uint cur_pos){
	uint bound = 0;
	for(int i = bd_pos -1; i >= 0 && domains[i][P] == cur_pos; i--)
		bound += MIN(domains[i][LL], domains[i][IRL]);
	return bound;
}

uchar partition(uchar *arr, uchar start, uchar len, const uchar *adjrow){
	uchar i = 0;
	for(uchar j = 0; j < len; j++){
		if(adjrow[arr[start+j]]){
			uchar_swap(&arr[start + i], &arr[start + j]);
			i++;
		}
	}
	return i;
}

void generate_next_domains(uchar domains[][BDS], uint *bd_pos, uint cur_pos, uchar *left, uchar *right, uchar v, uchar w, uint inc_pos){
	int i;
	uint bd_backup = *bd_pos;
	uint bound = 0;
	uchar *bd;
	for(i = *bd_pos-1, bd = &domains[i][L]; i >= 0 && bd[P] == cur_pos-1; i--, bd = &domains[i][L]){

		uchar l_len = partition(left, bd[L], bd[LL], adjmat0[v]);
		uchar r_len = partition(right, bd[R], bd[RL], adjmat1[w]);

		if(bd[LL] - l_len && bd[RL] - r_len){
			add_bidomain(domains, bd_pos, bd[L] + l_len, bd[R] + r_len, bd[LL] - l_len, bd[RL]  - r_len, bd[ADJ], (uchar)(cur_pos));
			bound += MIN(bd[LL] - l_len, bd[RL]  - r_len);
		}
		if(l_len && r_len){
			add_bidomain(domains, bd_pos, bd[L], bd[R], l_len, r_len, true, (uchar)(cur_pos));
			bound += MIN(l_len, r_len);
		}
	}
	if (cur_pos + bound <= inc_pos)  *bd_pos = bd_backup;
}

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
uchar find_min_value(uchar *arr, uchar start_idx, uchar len){
	uchar min_v = UCHAR_MAX;
    for(int i = 0; i < len; i++){
        if(arr[start_idx+i] < min_v)
            min_v = arr[start_idx + i];
    }
    return min_v;
}

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

double compute_elapsed_sec(){
	struct timespec now;
	double time_elapsed;
	
	clock_gettime(CLOCK_MONOTONIC, &now);
	time_elapsed = (now.tv_sec - start.tv_sec);
	time_elapsed += (double)(now.tv_nsec - start.tv_nsec) / 1000000000.0;
	
	return time_elapsed;
}

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

void mcs(uchar incumbent[][2], uint *inc_pos){

	uint min = MIN(n0, n1);

	uchar cur[min][2];
	uchar domains[min*min][BDS];
	uchar left[n0], right[n1];
	uchar v, w, *bd;
	uint bd_pos = 0;
	for(uchar i = 0; i < n0; i++) left[i] = i;
	for(uchar i = 0; i < n1; i++) right[i] = i;
	add_bidomain(domains, &bd_pos, 0, 0, n0, n1, 0, 0);

	while (bd_pos > 0) {
		if (arguments.timeout && compute_elapsed_sec() > arguments.timeout) {
        	arguments.timeout = -1;
        	return;
   		}
		
		bd = &domains[bd_pos - 1][L];
		if (calc_bound(domains, bd_pos, bd[P]) + bd[P] <= *inc_pos || (bd[LL] == 0 && bd[RL] == bd[IRL])) {
			bd_pos--;
		} else {
			select_bidomain(domains, bd_pos, left, domains[bd_pos - 1][P], arguments.connected);
			v = select_next_v(left, bd);
			if ((bd[W] = select_next_w(right, bd)) != UCHAR_MAX) {
				w = right[bd[R] + bd[W]];       // swap the W after the bottom of the current right domain
				right[bd[R] + bd[W]] = right[bd[R] + bd[RL]];
				right[bd[R] + bd[RL]] = w;
				bd[W] = w;                      // store the W used for this iteration
				cur[bd[P]][L] = v;
				cur[bd[P]][R] = w;
				update_incumbent(cur, incumbent, bd[P] + (uchar) 1, inc_pos);
				generate_next_domains(domains, &bd_pos, bd[P] + 1, left, right, v, w, *inc_pos);
			}
		}
	}
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

	adjmat0 = g0->adjmat;
	adjmat1 = g1->adjmat;

	n0 = g0->n;
	n1 = g1->n;
	uint min_size = MIN(n0, n1);
	uchar solution[min_size][2];

	uint sol_len = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	mcs(solution, &sol_len);
	clock_gettime(CLOCK_MONOTONIC, &finish);


	

	if (!check_sol(g0, g1, solution, sol_len)) {
		fprintf(stderr, "*** Error: Invalid solution\n");
	}
	
	if (arguments.timeout == -1){
        printf("TIMEOUT\n");
	}
	
	printf("SOLUTION size:%d\nsol: ", sol_len);
	for(int i = 0; i < g0->n; i++)
		for(int j = 0; j < sol_len; j++)
			if(solution[j][L] == i)
				printf("|%2d %2d| ", solution[j][L], solution[j][R]);
	printf("\n");
	
	time_elapsed = (finish.tv_sec - start.tv_sec); // calculating elapsed seconds
	time_elapsed += (double)(finish.tv_nsec - start.tv_nsec) / 1000000000.0; // adding elapsed nanoseconds
	printf(">>> %d - %015.10f", sol_len, time_elapsed);

	free_graph(g0);
	free_graph(g1);
	return 0;
}
