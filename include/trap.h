#ifndef TRAP_H
#define TRAP_H

void trap_init(void);
void trap_handler(void);

void trigger_store_page_fault(unsigned long address);

unsigned long trap_get_last_cause(void);
unsigned long trap_get_last_value(void);

unsigned long trap_get_timer_ticks(void);

#endif
