#include "net_utils.h"

#include <sys/socket.h>
#include <errno.h>


int send_all(
    int socket_fd,
    const char *buffer,
    size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length) {

        ssize_t n = send(
            socket_fd,
            buffer + total_sent,
            length - total_sent,
            0
        );

        if (n < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (n == 0) {
            return -1;
        }

        total_sent += (size_t)n;
    }

    return 0;
}


ssize_t recv_line(
    int socket_fd,
    char *buffer,
    size_t buffer_size)
{
    size_t index = 0;

    while (index < buffer_size - 1) {

        char c;

        ssize_t n = recv(
            socket_fd,
            &c,
            1,
            0
        );

        if (n == 0) {

            if (index == 0) {
                return 0;
            }

            break;
        }

        if (n < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        buffer[index++] = c;

        if (c == '\n') {
            break;
        }
    }

    buffer[index] = '\0';

    return (ssize_t)index;
}
