#include "scheduler.h"
#include "../printf.h"

task_t tasks[MAX_TASKS];
int current_task = 0;
int task_count = 0;

