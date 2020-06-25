#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
	/* TODO: put a new process to queue [q] */	
	if (q->size<MAX_QUEUE_SIZE){	
		q->proc[q->size++]=proc;
	}
}

struct pcb_t * dequeue(struct queue_t * q) {
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	if (empty(q)){
		return NULL;
	}

	struct pcb_t * max_prio_proc = q->proc[0]; // init
	int max_prio_index = 0; // init

	// find process has highest priority
	for (int i=1; i<q->size; i++){ 
		if (q->proc[i]->priority > max_prio_proc->priority){
			max_prio_proc = q->proc[i];
			max_prio_index = i;
		}
	}
	// end find
	
	// remove pcb has highest priority
	q->proc[max_prio_index] = q->proc[q->size-1];
	q->size--;
	// end remove

	return max_prio_proc;
}

