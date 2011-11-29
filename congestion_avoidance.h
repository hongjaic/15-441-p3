#ifndef _CONGESTION_AVOIDANCE_H_
#define _CONGESTION_AVOIDANCE_H_

#include <stdlib.h>
#include <stdint.h>
#include "constants.h"
#include "macros.h"

typedef struct congestion_state_s
{
    int ca_state;
    uint32_t cwnd;
    uint32_t ssthresh;
    uint32_t curr_round;
} congestion_state_t;

void init_congestion_state(congestion_state_t *s);
void congestion_control(congestion_state_t *s);
void slow_start(congestion_state_t *s);
void mult_decrease(congestion_state_t *s);
void add_increase(congestion_state_t *s);

#endif /* _CONGESTION_AVOIDANCE_H_ */
