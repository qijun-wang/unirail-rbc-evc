#include "ma_generator.h"

#include <time.h>


#define D_TEST 0.5
#define VMAX_TEST 0.5
#define LEADER_LIMIT_TEST 20.0


static double get_timestamp(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    return (double)ts.tv_sec
         + (double)ts.tv_nsec / 1e9;
}


int generate_ma(
    RBCContext *context,
    uint32_t train_id,
    MovementAuthority *ma
)
{
    if (context == NULL || ma == NULL) {
        return -1;
    }

    if (train_id < 1 ||
        train_id > MAX_EVC) {

        return -1;
    }


    ma->train_id = train_id;
    ma->timestamp = get_timestamp();
    ma->vmax = VMAX_TEST;


    /* Train 1 = Leader */
    if (train_id == 1) {

        ma->movement_limit =
            LEADER_LIMIT_TEST;

        return 0;
    }


    int index = (int)train_id - 1;
    int previous_index = index - 1;


    pthread_mutex_lock(&context->mutex);


    if (!context->valid[previous_index]) {

        pthread_mutex_unlock(&context->mutex);

        return -1;
    }


    ma->movement_limit =
        context->trains[previous_index].position
        - D_TEST;


    pthread_mutex_unlock(&context->mutex);

    return 0;
}
