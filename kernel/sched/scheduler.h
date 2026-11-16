// Scheduler -- MaXOS, 16/11/26.
// Simple O(N) scheduler (how linux 0.11)
// Author: github.com/lev445
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../types.h"

// XXX: Add VMM here
// WIP: VMM is not created yet
#define MAX_TASKS 4
#define TASK_STACK_SIZE 4096

// Task state
#define TASK_RUNNING  0
#define TASK_SLEEPING 1
#define TASK_ZOMBIE   2

// task structure
typedef struct {
	uint64_t rsp; // stack ptr to task
	uint8_t stack[TASK_STACK_SIZE];
	int state; // task status
	int counter; // counter
	int priority; // Nice+
	int pid; // Process Id
	int active; // is active
} task_t;

void scheduler_init(void);
void scheduler_tick(void);
int spawn_task(void (*func)(void), int priority);

extern task_t tasks[MAX_TASKS];
extern int current_task;
extern int task_count;

#endif
