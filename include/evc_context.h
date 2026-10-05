#ifndef EVC_CONTEXT_H
#define EVC_CONTEXT_H

#include <stdint.h>
#include <pthread.h>

#include "protocol.h"


typedef struct {

    uint32_t train_id;

    double position;
    double measured_speed;

    MovementAuthority ma;
    int ma_valid;

    pthread_mutex_t mutex;

} EVCContext;


/* Initialize the EVC shared context */
int evc_context_init(
    EVCContext *context,
    uint32_t train_id
);


/* Destroy resources used by the context */
void evc_context_destroy(
    EVCContext *context
);


/* Update local train state */
int evc_context_update_state(
    EVCContext *context,
    double position,
    double measured_speed
);


/* Store a new Movement Authority */
int evc_context_update_ma(
    EVCContext *context,
    const MovementAuthority *ma
);


/* Display current EVC context */
void evc_context_print(
    EVCContext *context
);


#endif
