/*
 *  graphISO: Tools to compute the Maximum Common Subgraph between two graphs
 *  Copyright (c) 2019 Stefano Quer
 *  
 *  This program is free software : you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.If not, see < http: *www.gnu.org/licenses/>
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
