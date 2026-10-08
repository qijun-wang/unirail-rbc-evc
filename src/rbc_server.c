#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>

#include "rbc_context.h"
#include "ma_generator.h"

#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"
#include "net_utils.h"

#define RBC_PORT 5000
#define T_MAX_AGE 1.0


typedef struct {

    int client_fd;

    RBCContext *context;

} ThreadArgs;

double get_timestamp(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    return (double)ts.tv_sec
         + (double)ts.tv_nsec / 1e9;
}



/* =========================================================
   One communication thread for one EVC connection
   ========================================================= */

void *handle_evc(void *arg)
{
    
    ThreadArgs *args = (ThreadArgs *)arg;

    int client_fd = args->client_fd;
    RBCContext *context = args->context;
    
    free(arg);

    char buffer[MESSAGE_SIZE];

    uint32_t ma_sequence_number = 0;
    uint32_t last_state_sequence = 0;

    int first_state_received = 0;
    uint32_t bound_train_id = 0;
    uint32_t last_request_sequence = 0;
    int first_request_received = 0;


    printf(
        "[RBC] Communication thread started "
        "(socket=%d)\n",
        client_fd
    );


    while (1) {

        ssize_t n = recv_line(client_fd, buffer, sizeof(buffer));
        if (n == 0) {
            printf("[RBC] EVC disconnected (socket=%d)\n", client_fd);
            break;
        }
        if (n < 0) {
            perror("[RBC] recv");
            break;
        }

        if (strncmp(buffer, "STATE;", 6) == 0) {
            TrainState state;
            if (parse_train_state(buffer, &state) < 0) {
                printf("[RBC] Invalid STATE (socket=%d)\n", client_fd);
                continue;
            }
            if (state.train_id < 1 || state.train_id > MAX_EVC) {
                printf("[RBC] Invalid train_id: %u\n", state.train_id);
                continue;
            }
            if (bound_train_id != 0 && state.train_id != bound_train_id) {
                printf("[RBC] STATE rejected: train_id differs from connection identity\n");
                continue;
            }
            if (first_state_received) {
                if (state.sequence_number <= last_state_sequence) {
                    printf("[RBC] STATE rejected: duplicate/old sequence\n");
                    continue;
                }
                if (state.sequence_number > last_state_sequence + 1) {
                    printf("[RBC] WARNING: STATE sequence gap (expected %u, received %u)\n",
                           last_state_sequence + 1, state.sequence_number);
                }
            }
            double message_age = get_timestamp() - state.timestamp;
            printf("[RBC] STATE train=%u seq=%u pos=%.2f speed=%.2f age=%.3f ms\n",
                   state.train_id, state.sequence_number, state.position,
                   state.speed, message_age * 1000.0);
            if (message_age > T_MAX_AGE) {
                printf("[RBC] STATE rejected: message too old\n");
                continue;
            }
            /* Keep the first accepted STATE as the connection identity. */
            if (rbc_context_update(context, &state) < 0) {
                printf("[RBC] Cannot update context for Train %u\n", state.train_id);
                continue;
            }
            if (bound_train_id == 0) bound_train_id = state.train_id;
            last_state_sequence = state.sequence_number;
            first_state_received = 1;
            rbc_context_print(context);
            rbc_context_print_interdistances(context);
            /* STATE does not trigger a Movement Authority response. */
        } else if (strncmp(buffer, "MA_REQUEST;", 11) == 0) {
            MARequest request;
            if (parse_ma_request(buffer, &request) < 0) {
                printf("[RBC] Invalid MA_REQUEST\n");
                continue;
            }
            if (bound_train_id == 0 || request.train_id != bound_train_id) {
                printf("[RBC] MA_REQUEST rejected: no bound STATE or wrong train_id\n");
                continue;
            }
            if (first_request_received &&
                request.sequence_number <= last_request_sequence) {
                printf("[RBC] MA_REQUEST rejected: duplicate/old sequence\n");
                continue;
            }
            double request_age = get_timestamp() - request.timestamp;
            if (request_age > T_MAX_AGE) {
                printf("[RBC] MA_REQUEST rejected: message too old\n");
                continue;
            }
            /* V1.7: check that this train has a state in RBCContext.
               A separate per-train freshness check is still needed before
               using cached states for safety-critical MA computation. */
            pthread_mutex_lock(&context->mutex);
            int state_available = context->valid[request.train_id - 1];
            pthread_mutex_unlock(&context->mutex);
            if (!state_available) {
                printf("[RBC] MA_REQUEST rejected: no valid STATE\n");
                continue;
            }
            last_request_sequence = request.sequence_number;
            first_request_received = 1;
            printf("[RBC] MA_REQUEST train=%u seq=%u\n",
                   request.train_id, request.sequence_number);
            MovementAuthority ma = {0};
            if (generate_ma(context, request.train_id, &ma) < 0) {
                printf("[RBC] Cannot generate MA for Train %u\n", request.train_id);
                continue;
            }
            ma.sequence_number = ++ma_sequence_number;
            ma.timestamp = get_timestamp();
            char ma_buffer[MESSAGE_SIZE];
            int message_length = serialize_ma(&ma, ma_buffer, sizeof(ma_buffer));
            if (message_length < 0) {
                printf("[RBC] MA serialization error\n");
                continue;
            }
            if (send_all(client_fd, ma_buffer, (size_t)message_length) < 0) {
                perror("[RBC] send");
                break;
            }
            printf("[RBC] MA sent train=%u seq=%u limit=%.2f vmax=%.2f\n",
                   ma.train_id, ma.sequence_number, ma.movement_limit, ma.vmax);
        } else {
            printf("[RBC] Unknown message type (socket=%d): %s\n", client_fd, buffer);
        }
    }


    close(client_fd);

    return NULL;
}


/* =========================================================
   Main thread
   ========================================================= */

int main(void)
{

    RBCContext context;

    if (rbc_context_init(&context) < 0) {

        fprintf(
            stderr,
            "[RBC] Failed to initialize context\n"
        );

        return EXIT_FAILURE;
    }   
   
   
    int server_fd;

    struct sockaddr_in server_addr;


    /* -----------------------------------------
       Create listening socket
       ----------------------------------------- */

    server_fd =
        socket(AF_INET, SOCK_STREAM, 0);


    if (server_fd < 0) {

        perror("socket");
        return EXIT_FAILURE;
    }


    int opt = 1;

    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );


    /* -----------------------------------------
       Configure server address
       ----------------------------------------- */

    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );


    server_addr.sin_family = AF_INET;

    server_addr.sin_addr.s_addr =
        htonl(INADDR_ANY);

    server_addr.sin_port =
        htons(RBC_PORT);


    /* -----------------------------------------
       Bind
       ----------------------------------------- */

    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0) {

        perror("bind");

        close(server_fd);

        return EXIT_FAILURE;
    }


    /* -----------------------------------------
       Listen
       ----------------------------------------- */

    if (listen(server_fd, MAX_EVC) < 0) {

        perror("listen");

        close(server_fd);

        return EXIT_FAILURE;
    }


    printf(
        "[RBC] Server listening on port %d\n",
        RBC_PORT
    );


    /* -----------------------------------------
       Accept up to 3 EVC
       ----------------------------------------- */

    for (int i = 0; i < MAX_EVC; i++) {

        struct sockaddr_in client_addr;

        socklen_t client_addr_len =
            sizeof(client_addr);


        int client_fd =
            accept(
                server_fd,
                (struct sockaddr *)&client_addr,
                &client_addr_len
            );


        if (client_fd < 0) {

            perror("accept");
            continue;
        }


        printf(
            "[RBC] New EVC connection "
            "(socket=%d)\n",
            client_fd
        );


        /*
         * Allocate client_fd because each thread
         * needs its own argument.
         */

        ThreadArgs *args =
            malloc(sizeof(ThreadArgs));


	if (args == NULL) {

	    perror("malloc");

	    close(client_fd);

	    continue;
	}


	args->client_fd = client_fd;

	args->context = &context;

        /* -----------------------------------------
           Create communication thread
           ----------------------------------------- */

        pthread_t thread;


        if (pthread_create(
                &thread,
                NULL,
                handle_evc,
                args
            ) != 0) {

            perror("pthread_create");

            close(client_fd);

            free(args);

            continue;
        }


        /*
         * We do not need pthread_join()
         * for this V1 prototype.
         */

        pthread_detach(thread);
    }


    /*
     * Keep RBC alive while communication
     * threads are running.
     */

    while (1) {
        sleep(1);
    }


    close(server_fd);

    return EXIT_SUCCESS;
}
