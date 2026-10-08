#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "protocol.h"
#include "net_utils.h"
#include "evc_context.h"


#define RBC_PORT 5000
#define RBC_IP   "127.0.0.1"

#define SEND_PERIOD_MS 500
#define NUMBER_OF_MESSAGES 10
#define MA_TIMEOUT_MS 1000


static double get_timestamp(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    return (double)ts.tv_sec
         + (double)ts.tv_nsec / 1000000000.0;
}


static void sleep_ms(long milliseconds)
{
    struct timespec ts;

    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;

    nanosleep(&ts, NULL);
}


int main(int argc, char *argv[])
{
    /* ==========================================
       1. Read train ID
       ========================================== */

    if (argc != 2) {

        fprintf(
            stderr,
            "Usage: %s <train_id>\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }


    uint32_t train_id =
        (uint32_t)atoi(argv[1]);


    if (train_id < 1 || train_id > 3) {

        fprintf(
            stderr,
            "train_id must be 1, 2 or 3\n"
        );

        return EXIT_FAILURE;
    }


    /* ==========================================
       2. Initialize EVC shared context
       ========================================== */

    EVCContext context;


    if (evc_context_init(
            &context,
            train_id
        ) < 0) {

        fprintf(
            stderr,
            "[EVC %u] Failed to initialize context\n",
            train_id
        );

        return EXIT_FAILURE;
    }


    /* ==========================================
       3. Create TCP socket
       ========================================== */

    int socket_fd;

    struct sockaddr_in rbc_addr;

    char buffer[MESSAGE_SIZE];


    socket_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );


    if (socket_fd < 0) {

        perror("socket");

        evc_context_destroy(&context);

        return EXIT_FAILURE;
    }


    /* ==========================================
       4. Configure RBC address
       ========================================== */

    memset(
        &rbc_addr,
        0,
        sizeof(rbc_addr)
    );


    rbc_addr.sin_family = AF_INET;

    rbc_addr.sin_port =
        htons(RBC_PORT);


    if (inet_pton(
            AF_INET,
            RBC_IP,
            &rbc_addr.sin_addr
        ) <= 0) {

        printf(
            "Invalid RBC IP address.\n"
        );

        close(socket_fd);

        evc_context_destroy(&context);

        return EXIT_FAILURE;
    }


    /* ==========================================
       5. Connect to RBC
       ========================================== */

    if (connect(
            socket_fd,
            (struct sockaddr *)&rbc_addr,
            sizeof(rbc_addr)
        ) < 0) {

        perror("connect");

        close(socket_fd);

        evc_context_destroy(&context);

        return EXIT_FAILURE;
    }


    printf(
        "[EVC %u] Connected to RBC.\n\n",
        train_id
    );


    /* ==========================================
       6. Configure receive timeout
       ========================================== */

    struct timeval timeout;


    timeout.tv_sec =
        MA_TIMEOUT_MS / 1000;

    timeout.tv_usec =
        (MA_TIMEOUT_MS % 1000) * 1000;


    if (setsockopt(
            socket_fd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)
        ) < 0) {

        perror("setsockopt SO_RCVTIMEO");

        close(socket_fd);

        evc_context_destroy(&context);

        return EXIT_FAILURE;
    }


    /* ==========================================
       7. Prepare simulated TrainState
       ========================================== */

    TrainState state;
    uint32_t request_sequence_number = 0;


    state.train_id =
        train_id;

    state.sequence_number =
        0;

    /*
     * Simulation convention:
     *
     * Train 1 = Leader
     * Train 2 = Follower 1
     * Train 3 = Follower 2
     *
     * Direction of movement: increasing position
     */

    state.position =
        8.0 - 2.0 * train_id;

    state.speed =
        0.30;


    /*
     * Store initial local state
     * in the shared EVC context.
     */

    if (evc_context_update_state(
            &context,
            state.position,
            state.speed
        ) < 0) {

        printf(
            "[EVC %u] Failed to update local context.\n",
            train_id
        );
    }


    /* ==========================================
       8. Periodic communication loop
       ========================================== */

    for (int i = 0;
         i < NUMBER_OF_MESSAGES;
         i++) {

        /*
         * Update simulated train state
         */

        state.sequence_number++;

        /*
         * Simulate train movement:
         * +0.15 m every cycle
         */

        state.position += 0.15;


        /*
         * Update EVC shared context
         * with the new local state.
         */

        if (evc_context_update_state(
                &context,
                state.position,
                state.speed
            ) < 0) {

            printf(
                "[EVC %u] Failed to update local state.\n",
                train_id
            );

            break;
        }


        /*
         * Timestamp is generated just before
         * serialization and transmission.
         */

        state.timestamp =
            get_timestamp();


        /* --------------------------------------
           Serialize TrainState
           -------------------------------------- */

        int message_length =
            serialize_train_state(
                &state,
                buffer,
                sizeof(buffer)
            );


        if (message_length < 0) {

            printf(
                "[EVC %u] Failed to serialize "
                "TrainState.\n",
                train_id
            );

            break;
        }


        /* --------------------------------------
           Send TrainState
           -------------------------------------- */

        if (send_all(
                socket_fd,
                buffer,
                (size_t)message_length
            ) != 0) {

            perror("send");

            break;
        }


        printf(
            "[EVC %u] STATE sent    "
            "seq=%u  pos=%.2f m  "
            "speed=%.2f m/s\n",
            train_id,
            state.sequence_number,
            state.position,
            state.speed
        );


        /* --------------------------------------
           V1.7: Request a new Movement Authority

           Test policy: one MA_REQUEST per cycle.
           The final braking-curve trigger belongs
           to the control/safety integration.
           -------------------------------------- */

        MARequest request = {0};
        request.train_id = train_id;
        request.sequence_number = ++request_sequence_number;
        request.timestamp = get_timestamp();

        message_length = serialize_ma_request(
            &request, buffer, sizeof(buffer)
        );

        if (message_length < 0) {
            fprintf(stderr,
                    "[EVC %u] Failed to serialize MA_REQUEST.\n",
                    train_id);
            break;
        }

        if (send_all(socket_fd, buffer, (size_t)message_length) != 0) {
            perror("[EVC] send MA_REQUEST");
            break;
        }

        printf("[EVC %u] MA_REQUEST sent seq=%u\n",
               train_id, request.sequence_number);

        /* --------------------------------------
           Receive MA
           -------------------------------------- */

        ssize_t n =
            recv_line(
                socket_fd,
                buffer,
                sizeof(buffer)
            );


        if (n == 0) {

            printf(
                "[EVC %u] RBC closed connection.\n",
                train_id
            );

            break;
        }


        if (n < 0) {

            if (errno == EAGAIN ||
                errno == EWOULDBLOCK) {

                printf(
                    "[EVC %u] TIMEOUT: "
                    "no MA received within %d ms.\n\n",
                    train_id,
                    MA_TIMEOUT_MS
                );

                continue;
            }


            perror("recv");

            break;
        }


        /* --------------------------------------
           Parse MA
           -------------------------------------- */

        MovementAuthority ma;


        if (parse_ma(
                buffer,
                &ma
            ) != 0) {

            printf(
                "[EVC %u] Invalid MA received.\n",
                train_id
            );

            continue;
        }


        printf(
            "[EVC %u] MA received    "
            "seq=%u  limit=%.2f m  "
            "vmax=%.2f m/s\n",
            train_id,
            ma.sequence_number,
            ma.movement_limit,
            ma.vmax
        );


        /* --------------------------------------
           Store MA in EVC shared context
           -------------------------------------- */

        if (evc_context_update_ma(
                &context,
                &ma
            ) < 0) {

            printf(
                "[EVC %u] MA rejected: "
                "invalid train_id or context.\n\n",
                train_id
            );

            continue;
        }


        /* --------------------------------------
           Display current EVC shared context
           -------------------------------------- */

        evc_context_print(&context);


        printf("\n");


        /* --------------------------------------
           Wait until next communication cycle
           -------------------------------------- */

        sleep_ms(SEND_PERIOD_MS);
    }


    /* ==========================================
       9. Close TCP connection
       ========================================== */

    printf(
        "[EVC %u] Communication finished.\n",
        train_id
    );


    close(socket_fd);


    /* ==========================================
       10. Destroy EVC shared context
       ========================================== */

    evc_context_destroy(&context);


    return EXIT_SUCCESS;
}
