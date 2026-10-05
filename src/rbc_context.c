#include "rbc_context.h"

#include <stdio.h>
#include <string.h>


int rbc_context_init(RBCContext *context)
{
    if (context == NULL) {
        return -1;
    }

    memset(context, 0, sizeof(*context));

    if (pthread_mutex_init(
            &context->mutex,
            NULL
        ) != 0) {

        return -1;
    }

    return 0;
}


void rbc_context_destroy(RBCContext *context)
{
    if (context != NULL) {
        pthread_mutex_destroy(&context->mutex);
    }
}


int rbc_context_update(
    RBCContext *context,
    const TrainState *state
)
{
    if (context == NULL || state == NULL) {
        return -1;
    }

    if (state->train_id < 1 ||
        state->train_id > MAX_EVC) {

        return -1;
    }

    int index = (int)state->train_id - 1;

    pthread_mutex_lock(&context->mutex);

    context->trains[index] = *state;
    context->valid[index] = 1;

    pthread_mutex_unlock(&context->mutex);

    return 0;
}


void rbc_context_print(RBCContext *context)
{
    pthread_mutex_lock(&context->mutex);

    printf("\n[RBC] ===== GLOBAL STATE =====\n");

    for (int i = 0; i < MAX_EVC; i++) {

        if (context->valid[i]) {

            printf(
                "Train %d: pos=%.2f speed=%.2f seq=%u\n",
                i + 1,
                context->trains[i].position,
                context->trains[i].speed,
                context->trains[i].sequence_number
            );

        } else {

            printf(
                "Train %d: no valid state\n",
                i + 1
            );
        }
    }

    printf("[RBC] ========================\n");

    pthread_mutex_unlock(&context->mutex);
}


void rbc_context_print_interdistances(
    RBCContext *context
)
{
    pthread_mutex_lock(&context->mutex);

    if (context->valid[0] &&
        context->valid[1] &&
        context->valid[2]) {

        double d12 =
            context->trains[0].position -
            context->trains[1].position;

        double d23 =
            context->trains[1].position -
            context->trains[2].position;

        printf(
            "[RBC] Interdistance T1(Leader)-T2 = %.2f m\n",
            d12
        );

        printf(
            "[RBC] Interdistance T2-T3 = %.2f m\n",
            d23
        );
    }

    pthread_mutex_unlock(&context->mutex);
}
