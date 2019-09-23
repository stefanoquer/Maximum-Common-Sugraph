#include "threadpool.h"

static struct argp_option options[] = { { "quiet", 'q', 0, 0, "Quiet output" },
                                        { "verbose", 'v', 0, 0, "Verbose output" },
										{"lad", 'l', 0, 0, "Read LAD format"},
                                        { "connected", 'c', 0, 0, "Solve max common CONNECTED subgraph problem" },
                                        { "threads", 'n', 0, 0, "Number of threads used" },
                                        {"timeout", 't', "timeout", 0, "Set timeout of TIMEOUT milliseconds"},
                                        { 0 }
};

static char doc[] = "Find a maximum isomorphic graph";
static char args_doc[] = "FILENAME1 FILENAME2";
static struct {
    bool quiet;
    bool verbose;
    bool lad;
    int timeout;
    bool connected;
    uint n_threads;
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
    arguments.n_threads = DEFAULT_THREADS;
    arguments.filename1 = NULL;
    arguments.filename2 = NULL;
    arguments.arg_num = 0;
}
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    switch (key) {
        case 'q':
            arguments.quiet = true;
            break;
		case 't':
	    	arguments.timeout = strtol(arg, NULL, 10);
        break;
        case 'l':
        	arguments.lad = true;
        	break;
        case 'n':
	    	arguments.n_threads = strtol(arg, NULL, 10);
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
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}
static struct argp argp = { options, parse_opt, args_doc, doc };
struct timespec start;

void mcs(uchar incumbent[][2], uchar *inc_pos) {
    uint bd_pos = 0, bd_n = 0;
    uchar cur[MAX_GRAPH_SIZE][2];
    uchar domains[MAX_GRAPH_SIZE * 5][BDS];
    uchar left[n0], right[n1];
    uchar v, w, *bd;
    for (uchar i = 0; i < n0; i++)
        left[i] = i;
    for (uchar i = 0; i < n1; i++)
        right[i] = i;
    add_bidomain(domains, &bd_pos, 0, 0, n0, n1, 0, 0);

    pool_t *pool = init_pool(arguments.n_threads, arguments.timeout, start, arguments.connected);

    while (bd_pos > 0) {
    
    	if (arguments.timeout && compute_elapsed_millisec(start) > arguments.timeout) {
        	arguments.timeout = -1;
        	break;
   		}
    
		    bd = &domains[bd_pos - 1][L];

		    if (calc_bound(domains, bd_pos, bd[P], &bd_n) + bd[P] <= *inc_pos || (bd[LL] == 0 && bd[RL] == bd[IRL]))
		        bd_pos--;
		    else {

		    	select_bidomain(domains, bd_pos, left, domains[bd_pos - 1][P], arguments.connected);

		        if(bd[P]==POOL_LEVEL){
		            if(fill_pool_args(pool, domains, cur, left, right, &bd_pos, bd_n, *inc_pos))
		                compute(pool, inc_pos, incumbent);
		        } else {
		            v = select_next_v(left, bd);
		            if ((bd[W] = select_next_w(right, bd)) != UCHAR_MAX) {
		                w = right[bd[R] + bd[W]];       // swap the W after the bottom of the current right domain
		                right[bd[R] + bd[W]] = right[bd[R] + bd[RL]];
		                right[bd[R] + bd[RL]] = w;
		                bd[W] = w;                      // store the W used for this iteration
		                cur[bd[P]][L] = v;
		                cur[bd[P]][R] = w;
		                update_incumbent(cur, incumbent, bd[P] + (uchar) 1, inc_pos, DEFAULT_THREADS);
		                generate_next_domains(domains, &bd_pos, bd[P] + 1, left, right, v, w, *inc_pos);
		            }
		        }
		    }

    }
    if(arguments.timeout >= 0 && pool->n_th > 0)
        compute(pool, inc_pos, incumbent);

    arguments.timeout = stop_pool(pool);
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
    g0 = sort_vertices_by_degree(g0, (graph_edge_count(g1) > g1->n * (g1->n - 1) / 2));
    g1 = sort_vertices_by_degree(g1, (graph_edge_count(g0) > g0->n * (g0->n - 1) / 2));
    int min_size = MIN(g0->n, g1->n);
    n0 = g0->n;
    n1 = g1->n;

    for (int i = 0; i < n0; i++){
        for (int j = 0; j < n0; j++)
            adjmat0[i][j] = g0->adjmat[i][j];
	}
    for (int i = 0; i < n1; i++){
        for (int j = 0; j < n1; j++)
            adjmat1[i][j] = g1->adjmat[i][j];
	}
    uchar solution[min_size][2];
    uchar sol_len = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    mcs(solution, &sol_len);
    clock_gettime(CLOCK_MONOTONIC, &finish);

    if(arguments.timeout == -1)
    	printf("TIMEOUT\n");

    printf("SOLUTION size:%d\nsol: ", sol_len);
    for (int i = 0; i < g0->n; i++)
    for (int j = 0; j < sol_len; j++){
        if (solution[j][L] == i)
            printf("|%2d %2d| ", solution[j][L], solution[j][R]);
	}
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
