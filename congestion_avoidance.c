#include "congestion_avoidance.h"

void init_congestion_state(congestion_state_t *s)
{
    if (s == NULL)
    {
        return;
    }

    s->ca_state = IN_SS;
    s->cwnd = 1;
    s->ssthresh = MAXWIN;
    s->curr_round = 0;
}

void slow_start(congestion_state_t *s)
{
    uint32_t cwnd;

    if (s == NULL)
    {
        return;
    }

    if (s->ca_state != IN_SS)
    {
        return;
    }

    cwnd = s->cwnd;

    s->cwnd = MIN(cwnd + 1, s->ssthresh);

    if (s->cwnd == s->ssthresh)
    {
        s->ca_state = IN_CA;
        s->cwnd = 1;
        s->ssthresh = (uint32_t)(s->ssthresh/2);
        s->curr_round = 0;
    }
}

void mult_decrease(congestion_state_t *s)
{
    if (s == NULL)
    {
        return;
    }

    s->ca_state = IN_SS;
    s->ssthresh = MAX(2, (uint32_t)(s->cwnd/2));
    s->ssthresh = MIN(s->ssthresh, MAXWIN);
    s->curr_round = 0;
    s->cwnd = 1;
}

void add_increase(congestion_state_t *s)
{
    if (s == NULL)
    {
        return;
    }

    if (s->ca_state != IN_CA)
    {
        return;
    }

    if (s->curr_round != s->cwnd)
    {
        return;
    }

    if (s->cwnd == MAXWIN)
    {
        s->curr_round = 0;
        return;
    }

    s->curr_round = 0;
    s->cwnd++;
}
