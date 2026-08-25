#ifndef RESEARCH_H
#define RESEARCH_H

void research_reset(void);

void research_record_context_switch(void);

void research_record_sfence(
    unsigned long count
);

void research_record_address_space_switch(
    unsigned long ticks
);

void research_print_summary(
    unsigned long process_1_work_units,
    unsigned long process_2_work_units
);

#endif
