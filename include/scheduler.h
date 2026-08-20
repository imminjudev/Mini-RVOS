#ifndef SCHEDULER_H
#define SCHEDULER_H

int scheduler_init(void);
void scheduler_start(void);

unsigned long scheduler_task_runs(unsigned long id);

#endif
