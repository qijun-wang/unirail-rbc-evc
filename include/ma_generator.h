#ifndef MA_GENERATOR_H
#define MA_GENERATOR_H

#include <stdint.h>

#include "protocol.h"
#include "rbc_context.h"


int generate_ma(
    RBCContext *context,
    uint32_t train_id,
    MovementAuthority *ma
);


#endif
