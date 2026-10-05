#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stddef.h>
#include <sys/types.h>

int send_all(
    int socket_fd,
    const char *buffer,
    size_t length
);

ssize_t recv_line(
    int socket_fd,
    char *buffer,
    size_t buffer_size
);

#endif
