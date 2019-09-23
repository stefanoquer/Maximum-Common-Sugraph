//
// Created by gab on 08/01/19.
//

#include "utils.h"

void *safe_realloc(void* old, uint new_size){
    void *tmp = realloc(old, new_size);
    if (tmp != NULL) return tmp;
    else exit(-1);
}

void uchar_swap(uchar *a, uchar *b) {
    uchar tmp = *a;
    *a = *b;
    *b = tmp;
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


void update_incumbent(uchar cur[][2], uchar inc[][2], uchar cur_pos, uchar *inc_pos, uint th_idx) {
    if (cur_pos > *inc_pos) {
        *inc_pos = cur_pos;
        for (int i = 0; i < cur_pos; i++) {
            inc[i][L] = cur[i][L];
            inc[i][R] = cur[i][R];
        }
    }
}

// BIDOMAINS FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////////
void add_bidomain(uchar (*domains)[BDS], uint *bd_pos, uchar left_i,
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

uint calc_bound(uchar domains[][BDS], uint bd_pos,
                                     uint cur_pos, uint *bd_n) {
    uint bound = 0;
    int i;
    for (i = bd_pos - 1; i >= 0 && domains[i][P] == cur_pos; i--)
        bound += MIN(domains[i][LL], domains[i][IRL]);
    *bd_n = bd_pos - 1 - i;
    return bound;
}

uchar partition(uchar *arr, uchar start, uchar len,
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

void generate_next_domains(uchar domains[][BDS], uint *bd_pos, uint cur_pos, uchar *left, uchar *right, uchar v, uchar w, uint inc_pos) {
    int i;
    uint bd_backup = *bd_pos;
    uint bound = 0;
    uchar *bd;
    for (i = *bd_pos - 1, bd = &domains[i][L]; i >= 0 && bd[P] == cur_pos - 1; i--, bd = &domains[i][L]) {

        uchar l_len = partition(left, bd[L], bd[LL], adjmat0[v]);
        uchar r_len = partition(right, bd[R], bd[RL], adjmat1[w]);

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

double compute_elapsed_millisec(struct timespec start){
	struct timespec now;
	double time_elapsed;

	clock_gettime(CLOCK_MONOTONIC, &now);
	time_elapsed = (now.tv_sec - start.tv_sec);
	time_elapsed += (double)(now.tv_nsec - start.tv_nsec) / 1000000000.0;

	return time_elapsed;
}




