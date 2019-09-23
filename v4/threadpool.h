//
// Created by gab on 08/01/19.
//

#ifndef TRIMBLE_IT_MULTI_THREADPOOL_H
#define TRIMBLE_IT_MULTI_THREADPOOL_H

#include "utils.h"

#define MAX_THREADS 64


typedef struct task_data_S{
    uchar domains[MAX_GRAPH_SIZE*5][BDS];
    uint bd_pos;
    uchar left[MAX_GRAPH_SIZE], right[MAX_GRAPH_SIZE];
    uchar current[MAX_GRAPH_SIZE][2];
    uint start_inc_size;
}task_data_t;

typedef struct thradpool{
    pthread_t *threads;
    uint pool_size;

    task_data_t *args;    //array of arguments from the main thread
    uint args_n;   //number of arguments (each of size args_size) in args array

    bool connected;

    pthread_mutex_t idle_mtx, ready_mtx, working_mtx, finish_mtx;
    pthread_cond_t  idle_cv, ready_cv, working_cv, finish_cv;
    int currentlyIdle, workReady, currentlyWorking, canFinish;

    uint n_th;      // the current index in the args_indices array

    int timeout;
    struct timespec start;

    pthread_mutex_t inc_mtx;
    uint global_inc;

    uchar inc_size[MAX_THREADS];
    uchar incumbents[MAX_THREADS][MAX_GRAPH_SIZE][2];

    bool stop;


}pool_t;

typedef struct thread_args_s{
    pool_t *pool;
    uint idx;
} thread_args_t;

pool_t *init_pool(uint pool_size, int timeout, struct timespec start, bool connected);

bool fill_pool_args(pool_t *pool, uchar (*domains)[BDS], uchar (*current)[2], const uchar *left, const uchar *right, uint *bd_pos, uint bd_n, uint inc_size);

void compute(pool_t *pool, uchar *inc_size, uchar (*incumbent)[2]);

int stop_pool(pool_t *pool);



#endif //TRIMBLE_IT_MULTI_THREADPOOL_H
