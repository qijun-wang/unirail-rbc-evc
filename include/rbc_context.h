#ifndef RBC_CONTEXT_H
#define RBC_CONTEXT_H

#include <stdint.h>
#include <pthread.h>

#include "protocol.h"

#define MAX_EVC 3


typedef struct {

    TrainState trains[MAX_EVC];
    int valid[MAX_EVC];

    pthread_mutex_t mutex;

} RBCContext;


/* Initialize the global RBC context */
int rbc_context_init(RBCContext *context);


/* Destroy resources used by the context */
void rbc_context_destroy(RBCContext *context);


/* Update the latest valid state of one train */
int rbc_context_update(
    RBCContext *context,
    const TrainState *state
);


/* Display current global state */
void rbc_context_print(
    RBCContext *context
);


/* Display current inter-train distances */
void rbc_context_print_interdistances(
    RBCContext *context
);


#endif
