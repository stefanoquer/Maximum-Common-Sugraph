//
// Created by gab on 08/01/19.
//

#include "threadpool.h"

void *func(void *args);




pool_t *init_pool(uint pool_size, int timeout, struct timespec start, bool connected) {
	pool_t *pool = malloc(sizeof *pool);
	pool->pool_size = pool_size;
	pool->args_n = 2 * pool->pool_size; // initial average of two domains per thread
	pool->args = malloc(pool->args_n * sizeof *pool->args);
	pool->threads = malloc(pool->pool_size * sizeof *pool->threads);
	pool->n_th = 0;

	pool->timeout = timeout;
	pool->start = start;

	pool->connected = connected;

	pthread_mutex_init(&pool->idle_mtx, NULL);
	pthread_cond_init(&pool->idle_cv, NULL);
	pool->currentlyIdle = 0;
	pthread_mutex_init(&pool->ready_mtx, NULL);
	pthread_cond_init(&pool->ready_cv, NULL);
	pool->workReady = 0;
	pthread_mutex_init(&pool->working_mtx, NULL);
	pthread_cond_init(&pool->working_cv, NULL);
	pool->currentlyWorking = 0;
	pthread_mutex_init(&pool->finish_mtx, NULL);
	pthread_cond_init(&pool->finish_cv, NULL);
	pool->canFinish = 0;
	pthread_mutex_init(&pool->inc_mtx, NULL);
	pool->global_inc = 0;
	//pool->inc_size = malloc(pool_size*sizeof *pool->inc_size);
	//pool->incumbents = malloc(pool_size*sizeof *pool->incumbents);


	for (uint i = 0; i < pool->pool_size; i++) {
		thread_args_t *args = malloc(sizeof *args);
		args->idx=i;
		args->pool=pool;
		pthread_create(&pool->threads[i], NULL, func, (void *) args);
	}

	return pool;
}

void *func(void *args) {
	thread_args_t *a = (thread_args_t*)args;
	uint my_idx = a->idx;
	pool_t *pool = a->pool;

	uchar v, w, *bd, *left, *right, (*domains)[BDS];
	uchar (*cur)[2] = pool->args[my_idx].current;
	uint bd_n, bd_pos;

	while (1) {

		// Set yourself as idle and signal to the main thread, when all threads are idle main will start
		pthread_mutex_lock(&pool->idle_mtx);
		pool->currentlyIdle++;
		pthread_cond_signal(&pool->idle_cv);
		pthread_mutex_unlock(&pool->idle_mtx);

		// wait for work from main
		pthread_mutex_lock(&pool->ready_mtx);
		while (!pool->workReady) {
			pthread_cond_wait(&pool->ready_cv , &pool->ready_mtx);
		}
		pthread_mutex_unlock(&pool->ready_mtx);


		if (!pool->stop) {
			if(my_idx < pool->n_th) {
				domains = pool->args[my_idx].domains;
				bd_pos = pool->args[my_idx].bd_pos;
				left = pool->args[my_idx].left;
				right = pool->args[my_idx].right;

				pool->inc_size[my_idx] = pool->args[my_idx].start_inc_size;

				while (bd_pos > 0) {

					if (pool->timeout && compute_elapsed_millisec(pool->start) > pool->timeout) {
						pool->timeout = -1;
						break;
					}

					bd = &domains[bd_pos - 1][L];

					if (calc_bound(domains, bd_pos, bd[P], &bd_n) + bd[P] <= pool->inc_size[my_idx] ||
							(bd[LL] == 0 && bd[RL] == bd[IRL]))
						bd_pos--;
					else {
						select_bidomain(domains, bd_pos, left, domains[bd_pos - 1][P], pool->connected);
						v = select_next_v(left, bd);
						if ((bd[W] = select_next_w(right, bd)) != UCHAR_MAX) {
							w = right[bd[R] + bd[W]];       // swap the W after the bottom of the current right domain
							right[bd[R] + bd[W]] = right[bd[R] + bd[RL]];
							right[bd[R] + bd[RL]] = w;
							bd[W] = w;                      // store the W used for this iteration
							cur[bd[P]][L] = v;
							cur[bd[P]][R] = w;
							pthread_mutex_lock(&pool->inc_mtx);
							if(bd[P]+1 > pool->global_inc){
								pool->global_inc = bd[P];
								pthread_mutex_unlock(&pool->inc_mtx);
								update_incumbent(cur, pool->incumbents[my_idx], bd[P] + (uchar) 1, &pool->inc_size[my_idx], my_idx);
							} else pthread_mutex_unlock(&pool->inc_mtx);


							generate_next_domains(domains, &bd_pos, bd[P] + 1, left, right, v, w, pool->inc_size[my_idx]);
						}
					}
				}
			} else pool->inc_size[my_idx] = 0;
		} else return NULL;

		// mark yourself as finished and signal to main
		pthread_mutex_lock(&pool->working_mtx);
		pool->currentlyWorking--;
		pthread_cond_signal(&pool->working_cv);
		pthread_mutex_unlock(&pool->working_mtx);

		// Wait for permission to finish
		pthread_mutex_lock(&pool->finish_mtx);
		while (!pool->canFinish) {
			pthread_cond_wait(&pool->finish_cv , &pool->finish_mtx);
		}
		pthread_mutex_unlock(&pool->finish_mtx);


	}

}

void compute(pool_t *pool, uchar *inc_size, uchar (*incumbent)[2]) {

	pthread_mutex_lock(&pool->idle_mtx);
	while (pool->currentlyIdle != pool->pool_size) {
		pthread_cond_wait(&pool->idle_cv, &pool->idle_mtx);
	}
	pthread_mutex_unlock(&pool->idle_mtx);

	pool->canFinish = 0;
	// Reset the number of currentlyWorking threads
	pool->currentlyWorking = pool->pool_size;

	// Signal to the threads to start
	pthread_mutex_lock(&pool->ready_mtx);
	pool->workReady = 1;
	pthread_cond_broadcast(&pool->ready_cv );
	pthread_mutex_unlock(&pool->ready_mtx);

	// Wait for them to finish
	pthread_mutex_lock(&pool->working_mtx);
	while (pool->currentlyWorking != 0) {
		pthread_cond_wait(&pool->working_cv, &pool->working_mtx);
	}
	pthread_mutex_unlock(&pool->working_mtx);

	// The threads are now waiting for permission to finish
	// Prevent them from starting again
	pool->workReady = 0;
	pool->currentlyIdle = 0;

	// Allow them to finish
	pthread_mutex_lock(&pool->finish_mtx);
	pool->canFinish = 1;
	pthread_cond_broadcast(&pool->finish_cv);
	pthread_mutex_unlock(&pool->finish_mtx);

	for(int i = 0; i < pool->n_th; i++){
		update_incumbent(pool->incumbents[i], incumbent, pool->inc_size[i], inc_size, DEFAULT_THREADS);
	}


	//retrieve result from pool solution
	pool->n_th = 0;
}

int stop_pool(pool_t *pool){
	pthread_mutex_lock(&pool->idle_mtx);
	while (pool->currentlyIdle != pool->pool_size) {
		pthread_cond_wait(&pool->idle_cv, &pool->idle_mtx);
	}
	pthread_mutex_unlock(&pool->idle_mtx);


	pool->canFinish = 1;
	// Reset the number of currentlyWorking threads
	pool->currentlyWorking = pool->pool_size;
	pool->stop = true;

	// Signal to the threads to start
	pthread_mutex_lock(&pool->ready_mtx);
	pool->workReady = 1;
	pthread_cond_broadcast(&pool->ready_cv );
	pthread_mutex_unlock(&pool->ready_mtx);

	for(int i = 0; i < pool->pool_size; i++){
		pthread_join(pool->threads[i], NULL);
	}
	return pool->timeout;
}

bool fill_pool_args(pool_t *pool, uchar (*domains)[BDS], uchar (*current)[2], const uchar *left, const uchar *right, uint *bd_pos, uint bd_n, uint inc_size){
	pool->args[pool->n_th].bd_pos = 0;
	for(uint i = 0; i < bd_n; i++, (*bd_pos)--)
		add_bidomain(pool->args[pool->n_th].domains, &pool->args[pool->n_th].bd_pos, domains[*bd_pos-1][L], domains[*bd_pos-1][R], domains[*bd_pos-1][LL], domains[*bd_pos-1][RL], domains[*bd_pos-1][ADJ], domains[*bd_pos-1][P]);
	for(int b = 0; b < POOL_LEVEL; b++)
		pool->args[pool->n_th].current[b][L] = current[b][L];
	for(int b = 0; b < POOL_LEVEL; b++)
		pool->args[pool->n_th].current[b][R] = current[b][R];
	for(int b = 0; b < n0; b++)
		pool->args[pool->n_th].left[b] = left[b];
	for(int b = 0; b < n1; b++)
		pool->args[pool->n_th].right[b] = right[b];
	pool->args[pool->n_th].start_inc_size = inc_size;
	pool->n_th++;

	return (pool->n_th == pool->pool_size);
}
