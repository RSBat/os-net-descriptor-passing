#include "fd_wrapper.h"

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>


using std::cout;
using std::endl;
using std::cerr;
using std::string;

const char* SOCKET_ADDRESS = "/tmp/kozelko-os-descriptor-passing-socket";

#define ENABLE_LOGGING

static const size_t BUFFER_SIZE = 1024;

void log(std::string const& message) {
#ifdef ENABLE_LOGGING
    cerr << message << endl;
#endif
}

void log(char* message) {
#ifdef ENABLE_LOGGING
    cerr << message << endl;
#endif
}

void log(int message) {
#ifdef ENABLE_LOGGING
    cerr << message << endl;
#endif
}

void print_error(const std::string &comment) {
    cerr << comment << ": " << strerror(errno) << endl;
}

ssize_t send_fd(int socket_fd, int fd) {
    msghdr msg{};
    char buf[CMSG_SPACE(sizeof(fd))];
    memset(buf, 0, sizeof(buf));

    // for some reason won't work without this
    iovec io{};
    const char* tmp = "a";
    io.iov_base = (void *) tmp;
    io.iov_len = 1;

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

    memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

    msg.msg_controllen = CMSG_LEN(sizeof(fd));

    ssize_t status = sendmsg(socket_fd, &msg, 0);
    return status;
}

int main() {
    Fd_wrapper server_socket(socket(AF_UNIX, SOCK_STREAM, 0));

    if (server_socket == -1) {
        print_error("Could not create socket");
        return 1;
    }

    sockaddr_un serv_addr{};
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, SOCKET_ADDRESS);

    if (bind(server_socket,
            reinterpret_cast<const sockaddr*>(&serv_addr),
            sizeof(sockaddr_un)) == -1) {
        print_error("Could not bind socket");
        return 2;
    }

    if (listen(server_socket, 10) == -1) {
        print_error("Could not listen to socket");
        return 3;
    }

    for (size_t iter = 0; iter < 10; iter++) {
        Fd_wrapper client_socket(accept(server_socket,
                                        nullptr,
                                        nullptr));
        if (client_socket == -1) {
            print_error("Could not accept connection");
            continue;
        }

        int pipe_fd[2];

        if (pipe(pipe_fd) == -1) {
            print_error("Could not create pipe for incoming data");
            continue;
        }

        Fd_wrapper pipe_in(pipe_fd[0]);

        log(pipe_fd[1]);
        if (send_fd(client_socket, pipe_fd[1]) == -1) {
            print_error("Could not send message with file descriptor");
            continue;
        }
        if (close(pipe_fd[1]) == -1) { // close write end
            print_error("Ignoring: could not close write end of pipe");
        }

        if (pipe(pipe_fd) == -1) {
            print_error("Could not create pipe for outgoing data");
            continue;
        }

        Fd_wrapper pipe_out(pipe_fd[1]);

        log(pipe_fd[0]);
        if (send_fd(client_socket, pipe_fd[0]) == -1) {
            print_error("Could not send message with file descriptor");
            continue;
        }
        if (close(pipe_fd[0]) == -1) { // close read end
            print_error("Ignoring: could not close read end of pipe");
        }

        char buf[BUFFER_SIZE];
        memset(buf, 0, BUFFER_SIZE);
        ssize_t bytes_read;
        while ((bytes_read = read(pipe_in, buf, BUFFER_SIZE)) > 0) {
            log(buf);

            while (bytes_read > 0) {
                ssize_t written = write(pipe_out, buf, bytes_read);
                if (written == -1) {
                    print_error("Could not write to pipe");
                    break;
                }
                bytes_read -= written;
            }
        }

        if (bytes_read == -1) {
            print_error("Could not read from pipe");
        }
    }

    log("Shutting down server");

    if (unlink(SOCKET_ADDRESS) == -1) {
        print_error("Could not delete socket");
    }

    return 0;
}