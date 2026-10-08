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

/* EVC -> RBC : Movement Authority Request */
typedef struct {
    uint32_t train_id;
    uint32_t sequence_number;
    double timestamp;
} MARequest;

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

/* MA Request serialization */
int serialize_ma_request(
    const MARequest *request,
    char *buffer,
    size_t buffer_size
);

/* MA Request parsing */
int parse_ma_request(
    const char *buffer,
    MARequest *request
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
