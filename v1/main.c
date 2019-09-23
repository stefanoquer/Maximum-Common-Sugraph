#define _GNU_SOURCE
#define _POSIX_SOURCE

#include "graph.h"

#include <argp.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned long long ULL;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}
static void fail(char* msg) {
    printf("%s\n", msg);
    exit(1);
}

static char doc[] = "Find a maximum clique in a graph in DIMACS format";
static char args_doc[] = "FILENAME1 FILENAME2";
static struct argp_option options[] = {
        {"quiet", 'q', 0, 0, "Quiet output"},
        {"verbose", 'v', 0, 0, "Verbose output"},
        {"lad", 'l', 0, 0, "Read LAD format"},
        {"timeout", 't', "timeout", 0, "Set timeout of TIMEOUT seconds"},
        {"connected", 'c', 0, 0, "Solve max common CONNECTED subgraph problem"},
        { 0 }
};

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
    arguments.connected = false;
    arguments.lad = false;
    arguments.timeout = 0;
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
        case 'v':
            arguments.verbose = true;
            break;
        case 't':
            arguments.timeout = strtol(arg, NULL, 10);
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
struct timespec start;

typedef struct vtx_pair_s {
    int v;
    int w;
}pair_t;

typedef struct vtx_pair_list_s {
    pair_t *vals;
    unsigned len;
    unsigned size;
}mapping_t;

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

void set_incumbent(mapping_t *current, mapping_t *incumbent){
    incumbent->len = current->len;
    if(arguments.verbose) printf("new Incumbent: ");
    for (int i=0; i<current->len; i++)
    {
        incumbent->vals[i] = current->vals[i];
        if(arguments.verbose) printf("|%d %d| ", incumbent->vals[i].v, incumbent->vals[i].w);
    }
    if(arguments.verbose)printf("\n");
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
    for (int i=0; i<domains->len; i++) {
        bidomain_t *old_bd = &domains->vals[i];
        int left_len = partition(left, old_bd->l, old_bd->left_len, g0->adjmat[v]);
        int right_len = partition(right, old_bd->r, old_bd->right_len, g1->adjmat[w]);
        int left_len_noedge = old_bd->left_len - left_len;
        int right_len_noedge = old_bd->right_len - right_len;
        if (left_len_noedge && right_len_noedge)
            add_bidomain(new_d, old_bd->l+left_len, old_bd->r+right_len, left_len_noedge, right_len_noedge, old_bd->is_adjacent);
        if (left_len && right_len)
            add_bidomain(new_d, old_bd->l, old_bd->r, left_len, right_len, true);
    }
    return new_d;
}

bool check_sol(graph_t *g0, graph_t *g1 , mapping_t *solution) {
    bool *used_left = calloc(g0->n, sizeof *used_left);
    bool *used_right = calloc(g1->n, sizeof *used_right);
    for (int i=0; i<solution->len; i++) {
        pair_t *p0 = &solution->vals[i];
        if (used_left[p0->v] || used_right[p0->w])
            return false;
        used_left[p0->v] = true;
        used_right[p0->w] = true;
        if (g0->label[p0->v] != g1->label[p0->w])
            return false;
        for (unsigned int j=i+1; j<solution->len; j++) {
            pair_t *p1 = &solution->vals[j];
            if (g0->adjmat[p0->v][p1->v] != g1->adjmat[p0->w][p1->w])
                return false;
        }
    }
    return true;
}


void free_domains(bidomain_list_t *old){
    free(old->vals);
    free(old);
}

void free_solution(mapping_t *old){
    free(old->vals);
    free(old);
}

double compute_elapsed_sec(struct timespec strt){
	struct timespec now;
	double time_elapsed;

	clock_gettime(CLOCK_MONOTONIC, &now);
	time_elapsed = (now.tv_sec - strt.tv_sec);
	time_elapsed += (double)(now.tv_nsec - strt.tv_nsec) / 1000000000.0;

	return time_elapsed;
}

void solve(graph_t *g0, graph_t *g1, mapping_t *my_incumbent, mapping_t *current, bidomain_list_t *domains, int*left, int*right){
	if (arguments.timeout && compute_elapsed_sec(start) > arguments.timeout) {
        arguments.timeout = -1;
   	}
    if (arguments.timeout == -1) return;
    
    if (my_incumbent->len < current->len) set_incumbent(current, my_incumbent);
    if (current->len + calc_bound(domains) <= my_incumbent->len) return;
    int bd_idx = select_bidomain(domains, left, current->len, arguments.connected);
    if(bd_idx == -1) return;
    bidomain_t *bd = &domains->vals[bd_idx];
    bd->right_len--;
    int v = find_min_value(left, bd->l, bd->left_len);
    remove_vtx_from_left_domain(left, &domains->vals[bd_idx], v);
    int w = -1;
    for(int i = 0; i < bd->right_len +1; i++){
        /* try to match vertex v */
        int idx = index_of_next_smallest(right, bd->r, bd->right_len + 1, w);
        w = right[bd->r + idx];
        right[bd->r + idx] = right[bd->r + bd->right_len];
        right[bd->r + bd->right_len] = w;

        bidomain_list_t *new_domains = filter_domains(domains, left, right, g0, g1, v, w);
        current->vals[current->len++] = (pair_t){.v=v, .w=w};
        solve(g0,g1, my_incumbent, current, new_domains, left, right);
        free_domains(new_domains);
        current->len--;
    }
    bd->right_len++;
    if (bd->left_len == 0) remove_bidomain(domains, bd_idx);
    solve(g0, g1, my_incumbent, current, domains, left, right);
}


mapping_t *mcs(graph_t *g0, graph_t *g1){

    unsigned int size = MIN(g0->n, g1->n);

    mapping_t *current = calloc(1, sizeof *current);
    current->size = size;
    current->vals = calloc(current->size, sizeof *current->vals);

    bidomain_list_t *domains = calloc(1, sizeof *domains);
    domains->size = size;
    domains->vals = calloc(current->size, sizeof *domains->vals);

    mapping_t *incumbent = calloc(1, sizeof *incumbent);
    incumbent->size = size;
    incumbent->vals = calloc(incumbent->size, sizeof *incumbent->vals);;

    int *left = malloc(g0->n* sizeof *left );  // the buffer of vertex indices for the left partitions
    int *right = malloc(g1->n* sizeof *right );  // the buffer of vertex indices for the right partitions
    int l = 0, r = 0;  // next free index in left and right

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

    solve(g0, g1, incumbent, current, domains, left, right);

    free(left);
    free(right);
    free_domains(domains);
    free(current);
    return incumbent;

}


int main(int argc, char** argv) {
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


	printf("timeout %d\n", arguments.timeout);


	clock_gettime(CLOCK_MONOTONIC, &start);

	mapping_t *solution = mcs(g0, g1);

    clock_gettime(CLOCK_MONOTONIC, &finish);

    if (!check_sol(g0, g1, solution)){
        fail("*** Error: Invalid solution\n");
	}
	if (arguments.timeout == -1){
        printf("TIMEOUT\n");
	}
	
	time_elapsed = (finish.tv_sec - start.tv_sec); // calculating elapsed seconds
	time_elapsed += (double)(finish.tv_nsec - start.tv_nsec) / 1000000000.0; // adding elapsed nanoseconds
	
    printf("Solution size %d\n", solution->len);
    for (int i=0; i<g0->n; i++)
        for (int j=0; j<solution->len; j++)
            if (solution->vals[j].v == i)
                printf("(%d -> %d) ", solution->vals[j].v, solution->vals[j].w);
    printf("\n");

    printf(">>> %d -  %015.10f\n", solution->len, time_elapsed);

    free_graph(g0);
    free_graph(g1);
    return 0;
}
