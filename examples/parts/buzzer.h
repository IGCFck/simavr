#ifndef __BUZZER__
#define __BUZZER__

#include "../../simavr/sim/sim_irq.h"
#include "../../simavr/sim/sim_avr.h"

typedef struct buzzer_t {
    avr_irq_t * irqs;
    avr_t * avr;
    char state;
    float voltage;
} buzzer_t ;

typedef struct cb_params_t {
    buzzer_t* buzzer;
    char pin;
    avr_t * avr;
} cb_params_t;

void buzzer_init(
    struct avr_t * avr,
    buzzer_t * buzzer,
    const char * name
);

void print_times();

#endif
