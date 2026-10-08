#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>


int serialize_train_state(
    const TrainState *state,
    char *buffer,
    size_t buffer_size)
{
    int n = snprintf(
        buffer,
        buffer_size,
        "STATE;%" PRIu32 ";%" PRIu32 ";%.3f;%.3f;%.3f\n",
        state->train_id,
        state->sequence_number,
        state->timestamp,
        state->position,
        state->speed
    );

    if (n < 0 || (size_t)n >= buffer_size) {
        return -1;
    }

    return n;
}


int serialize_ma(
    const MovementAuthority *ma,
    char *buffer,
    size_t buffer_size)
{
    int n = snprintf(
        buffer,
        buffer_size,
        "MA;%" PRIu32 ";%" PRIu32 ";%.3f;%.3f;%.3f\n",
        ma->train_id,
        ma->sequence_number,
        ma->timestamp,
        ma->movement_limit,
        ma->vmax
    );

    if (n < 0 || (size_t)n >= buffer_size) {
        return -1;
    }

    return n;
}


int parse_train_state(
    const char *buffer,
    TrainState *state)
{
    char type[16];

    int count = sscanf(
        buffer,
        "%15[^;];%" SCNu32 ";%" SCNu32 ";%lf;%lf;%lf",
        type,
        &state->train_id,
        &state->sequence_number,
        &state->timestamp,
        &state->position,
        &state->speed
    );

    if (count != 6) {
        return -1;
    }

    if (strcmp(type, "STATE") != 0) {
        return -1;
    }

    return 0;
}


int parse_ma(
    const char *buffer,
    MovementAuthority *ma)
{
    char type[16];

    int count = sscanf(
        buffer,
        "%15[^;];%" SCNu32 ";%" SCNu32 ";%lf;%lf;%lf",
        type,
        &ma->train_id,
        &ma->sequence_number,
        &ma->timestamp,
        &ma->movement_limit,
        &ma->vmax
    );

    if (count != 6) {
        return -1;
    }

    if (strcmp(type, "MA") != 0) {
        return -1;
    }

    return 0;
}
int serialize_ma_request(
    const MARequest *request,
    char *buffer,
    size_t buffer_size
)
{
    if (request == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    int n = snprintf(
        buffer,
        buffer_size,
        "MA_REQUEST;%u;%u;%.6f\n",
        request->train_id,
        request->sequence_number,
        request->timestamp
    );

    if (n < 0 || (size_t)n >= buffer_size) {
        return -1;
    }

    return n;
}

int parse_ma_request(
    const char *buffer,
    MARequest *request
)
{
    if (buffer == NULL || request == NULL) {
        return -1;
    }

    unsigned int train_id;
    unsigned int sequence_number;
    double timestamp;
    int consumed = 0;

    int matched = sscanf(
        buffer,
        "MA_REQUEST;%u;%u;%lf%n",
        &train_id,
        &sequence_number,
        &timestamp,
        &consumed
    );

    if (matched != 3 ||
        (buffer[consumed] != '\n' &&
         buffer[consumed] != '\0')) {
        return -1;
    }

    request->train_id = (uint32_t)train_id;
    request->sequence_number = (uint32_t)sequence_number;
    request->timestamp = timestamp;

    return 0;
}
