/**
 * CS 15-441 Computer Networks
 * Project: Congestion Control with BitTorrent
 *
 * @file    congestion_avoidance.c
 * @author  Hong Jai Cho <hongjaic>, Raul Gonzalez <rggonzal>
 */

#include "congestion_avoidance.h"

/**
 * init_congestion_state -
 * Initializes congestion controller.
 *
 * @param s congestion control state data structure
 */
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


/**
 * slow_start -
 * This function modifies window size based on the specifications of slow start.
 *
 * @param s congestion control state data structure
 */
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


/**
 * mult_decrease -
 * This function modifies window size based on the specification of
 * multiplicative decrease.
 *
 * @param s congestion control state data structure
 */
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


/**
 * add_increase -
 * This function modifies window size based on the specifications of additive
 * increase.
 *
 * @param s congestion control state data structure
 */
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
