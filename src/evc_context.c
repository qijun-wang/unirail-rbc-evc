#include "evc_context.h"

#include <stdio.h>
#include <string.h>


int evc_context_init(
    EVCContext *context,
    uint32_t train_id
)
{
    if (context == NULL) {
        return -1;
    }

    memset(context, 0, sizeof(*context));

    context->train_id = train_id;

    if (pthread_mutex_init(
            &context->mutex,
            NULL
        ) != 0) {

        return -1;
    }

    return 0;
}


void evc_context_destroy(
    EVCContext *context
)
{
    if (context != NULL) {
        pthread_mutex_destroy(&context->mutex);
    }
}


int evc_context_update_state(
    EVCContext *context,
    double position,
    double measured_speed
)
{
    if (context == NULL) {
        return -1;
    }

    pthread_mutex_lock(&context->mutex);

    context->position = position;
    context->measured_speed = measured_speed;

    pthread_mutex_unlock(&context->mutex);

    return 0;
}


int evc_context_update_ma(
    EVCContext *context,
    const MovementAuthority *ma
)
{
    if (context == NULL || ma == NULL) {
        return -1;
    }

    /*
     * The MA must correspond to this train.
     */
    if (ma->train_id != context->train_id) {
        return -1;
    }

    pthread_mutex_lock(&context->mutex);

    context->ma = *ma;
    context->ma_valid = 1;

    pthread_mutex_unlock(&context->mutex);

    return 0;
}


void evc_context_print(
    EVCContext *context
)
{
    if (context == NULL) {
        return;
    }

    pthread_mutex_lock(&context->mutex);

    printf(
        "[EVC %u] Context: "
        "position=%.2f speed=%.2f",
        context->train_id,
        context->position,
        context->measured_speed
    );

    if (context->ma_valid) {

        printf(
            " MA(limit=%.2f vmax=%.2f seq=%u)",
            context->ma.movement_limit,
            context->ma.vmax,
            context->ma.sequence_number
        );

    } else {

        printf(" MA=not available");
    }

    printf("\n");

    pthread_mutex_unlock(&context->mutex);
}
