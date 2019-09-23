/*
 * threadpool.c
 *
 *  Created on: Oct 15, 2018
 *      Author: gab
 */

#include "threadpool.h"


void* thread_func(void* params){
	threadpool_t *help_me = ((param_t*)params)->help_me;
	while(!atomic_load(&help_me->finish)){ // @suppress("Type cannot be resolved")
		pthread_mutex_lock(&help_me->general_mtx);

		bool did_something = false;
		for(node_t *node = help_me->head; node!= NULL; node=node->next){ // extract a task
			task_t *task = node->task;
			if(task!= NULL && task->func!=NULL){
				func f = task->func;
				task->pending++;
				pthread_mutex_unlock(&help_me->general_mtx);
				task->args->thread_idx = ((param_t*)params)->idx;
				(*f)(task->args); //execute the function

				pthread_mutex_lock(&help_me->general_mtx);
				task->func = NULL; //?
				if(--task->pending == 0) pthread_cond_broadcast(&help_me->cv);

				did_something = true;
				break;
			}
		}
		if ((! did_something) && (!atomic_load(&help_me->finish))){ // @suppress("Type cannot be resolved")
			pthread_cond_wait(&help_me->cv, &help_me->general_mtx);
		}
		pthread_mutex_unlock(&help_me->general_mtx);
	}
	return NULL;
}

threadpool_t* init_threadpool(unsigned threads_num){
	threadpool_t *help_me = malloc(sizeof *help_me);
	help_me->n_threads = threads_num;
	help_me->threads = malloc(threads_num * sizeof help_me->threads);
	pthread_cond_init(&help_me->cv, NULL);
	pthread_mutex_init(&help_me->general_mtx, NULL);
	help_me->task_num = 0;


	help_me->head = NULL;
	help_me->tail = NULL;

	atomic_init(&help_me->finish, false); // @suppress("Type cannot be resolved")
	help_me->params = malloc(threads_num * sizeof *help_me->params);
	for(int i = 0; i< threads_num; i++){
		help_me->params[i] = malloc(sizeof *help_me->params[i]);
		help_me->params[i]->help_me = help_me;
		help_me->params[i]->idx = i+1;
		pthread_create(&help_me->threads[i], NULL, thread_func, help_me->params[i]);
	}
	return help_me;
}

void kill_workers(threadpool_t *help_me){
	pthread_mutex_lock(&help_me->general_mtx);
	atomic_store(&help_me->finish, true); // @suppress("Type cannot be resolved")
	pthread_cond_broadcast(&help_me->cv);
	pthread_mutex_unlock(&help_me->general_mtx);

	for(int i = 0; i < help_me->n_threads; i++) {
		pthread_join(help_me->threads[i], NULL);
		free(help_me->params[i]);
	}
	free(help_me->params);
	free(help_me->threads);
	free(help_me);
}

bool compare_pos(position_t *a, position_t* b){
	if(a->depth < b->depth) return true;
	else if (a->depth > b->depth) return false;
	for(int i = 0; i < SPLIT_LEVEL + 1; i++){
		if(a->vals[i] < b->vals[i]) return true;
		else if(a->vals[i] > b->vals[i]) return false;
	}
	return false;
}

void enqueue(node_t **head_ref, position_t pos, task_t *task){
	node_t* new_node = malloc(sizeof *new_node);
	new_node->task = task;
	new_node->pos = pos;
	new_node->next = NULL;
	if(*head_ref == NULL) {
		*head_ref = new_node;
	} else {
		if(compare_pos(&new_node->pos, &(*head_ref)->pos)){
			new_node->next = *head_ref;
			*head_ref = new_node;
		} else{
			node_t *n;
			for(n = (*head_ref); n->next != NULL; n = n->next ){
				if(compare_pos(&new_node->pos, &n->next->pos)){
					new_node->next = n->next;
					n->next = new_node;
					break;
				}
			}
			/* new_node is the last */
			if(!compare_pos(&new_node->pos, &n->pos))
				n->next = new_node;
		}
	}
//	for(node_t *n = *head_ref; n != NULL; n = n->next){
//		printf("-> %d [%d,%d,%d,%d,%d] ", n->pos.depth, n->pos.vals[0], n->pos.vals[1], n->pos.vals[2], n->pos.vals[3], n->pos.vals[4]);
//	}printf("\n");
	return;
}

void dequeue(node_t **head_ref, task_t *task){
	if(*head_ref == NULL || task == NULL)
		return;
	node_t *t = NULL;

	if((*head_ref)->task == task){
		t = *head_ref;
		*head_ref = (*head_ref)->next;
	}
	for(node_t *del = *head_ref; del!= NULL && del->next!=NULL; del=del->next){
		if(del->next->task==task){
			t = del->next;
			del->next = del->next->next;
		}
	}

	free_helper_args(t->task->args);
	free(t->task);
	free(t);
	return;
}

void get_help_with(position_t pos, threadpool_t *help_me, func main_function, func helper_function , args_t *main_args, args_t *helper_args){
	task_t *task = malloc(sizeof *task);
	task->args=helper_args;
	task->func=helper_function;
	task->pending = 0;
	/* put the task in the queue so that helper threads can execute it */
	pthread_mutex_lock(&help_me->general_mtx);
	enqueue(&help_me->head, pos, task);
	pthread_cond_broadcast(&help_me->cv);
	pthread_mutex_unlock(&help_me->general_mtx);

	(*main_function)(main_args); /* launch task in the main thread */

	pthread_mutex_lock(&help_me->general_mtx);
	while(task->pending != 0) pthread_cond_wait(&help_me->cv, &help_me->general_mtx);
	dequeue(&help_me->head, task); // the tash has been executed, remove it from the queue
	pthread_mutex_unlock(&help_me->general_mtx);

}


void free_helper_args(args_t *args){
	free(args->left);
	free(args->right);
	free_solution(args->current);
	free_domains(args->domains);
	free(args);
}
