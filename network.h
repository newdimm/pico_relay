#ifndef RELAY_NETWORK_H
#define RELAY_NETWORK_H

#ifdef HAVE_WIRELESS

#include "pico/time.h"

typedef struct
{
    bool init_done;
    absolute_time_t wifi_connect_time;
    bool connection_started;
    bool connected;

    absolute_time_t ready_wait_started;
    bool ready;
} relay_network_t;

extern relay_network_t rnet;

extern void network_init(void);
extern void network_periodic(uint32_t timeout_time_ms);

#else /* HAVE_WIRELESS */

#define network_init()
#define network_periodic(timeout_time_ms)

#endif /* HAVE_WIRELESS */

#endif /* RELAY_NETWORK_H */
