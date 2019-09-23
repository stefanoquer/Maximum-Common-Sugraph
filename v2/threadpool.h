/*
 * threadpool.h
 *
 *  Created on: Oct 15, 2018
 *      Author: gab
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include "utils.h"

threadpool_t* init_threadpool(unsigned threads_num);

void get_help_with(position_t pos, threadpool_t *helpme, func main_function, func helper_function , args_t *main_args, args_t *helper_args);

void kill_workers(threadpool_t *helpme);


bool compare_pos(position_t *a, position_t* b);

void free_helper_args(args_t *args);


#endif /* THREADPOOL_H_ */
