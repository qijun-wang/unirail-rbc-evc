#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define MESSAGE_SIZE 256

/* EVC -> RBC */
typedef struct {
    uint32_t train_id;
    uint32_t sequence_number;
    double timestamp;
    double position;
    double speed;
} TrainState;

/* RBC -> EVC */
typedef struct {
    uint32_t train_id;
    uint32_t sequence_number;
    double timestamp;
    double movement_limit;
    double vmax;
} MovementAuthority;


/* Serialization */
int serialize_train_state(
    const TrainState *state,
    char *buffer,
    size_t buffer_size
);

int serialize_ma(
    const MovementAuthority *ma,
    char *buffer,
    size_t buffer_size
);


/* Parsing */
int parse_train_state(
    const char *buffer,
    TrainState *state
);

int parse_ma(
    const char *buffer,
    MovementAuthority *ma
);

#endif
