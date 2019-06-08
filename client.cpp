#include "fd_wrapper.h"

#include <sys/socket.h>
#include <sys/socket.h>
#include <string>
#include <sys/un.h>
#include <iostream>

using std::string;
using std::cerr;
using std::cout;
using std::endl;

const char* SOCKET_ADDRESS = "/tmp/kozelko-os-descriptor-passing-socket";
const char* ECHO_MESSAGE = "Echo sample message";
const size_t MESAGE_SIZE = 19;

void print_error(const std::string &comment) {
    cerr << comment << ": " << strerror(errno) << endl;
}

int receive_fd(int socket_fd) {
    msghdr msg{};

    char msg_buf[256];
    iovec io{};
    io.iov_base = msg_buf;
    io.iov_len = sizeof(msg_buf);

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char control_buf[256];
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);

    ssize_t status = recvmsg(socket_fd, &msg, 0);

    if (status == -1) {
        print_error("Could not receive message with file descriptor");
        return -1;
    }

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);

    int fd;
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));

    return fd;
}

int main() {
    Fd_wrapper client_socket(socket(AF_UNIX, SOCK_STREAM, 0));
    if (client_socket == -1) {
        print_error("Could not create socket");
        return 1;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_ADDRESS);

    if (connect(client_socket,
            reinterpret_cast<const sockaddr *>(&address),
            sizeof(sockaddr_un)) == -1) {
        print_error("Could not connect to socket");
        return 2;
    }

    Fd_wrapper pipe_out(receive_fd(client_socket));
    Fd_wrapper pipe_in(receive_fd(client_socket));

    if (pipe_in == -1 || pipe_out == -1) {
        return 3;
    }

    cerr << "Received fds: " << pipe_out << " " << pipe_in << endl;

    const char* out_ptr = ECHO_MESSAGE;
    ssize_t to_write = MESAGE_SIZE;

    while (to_write > 0) {
        ssize_t written = write(pipe_out, out_ptr, to_write);
        if (written == -1) {
            print_error("Could not write to pipe");
            return 4;
        }
        to_write -= written;
        out_ptr += written;
    }

    char buf[1024];
    memset(buf, 0, 1024);
    char* in_ptr = buf;
    ssize_t to_read = 19;

    while (to_read > 0) {
        ssize_t bytes_read = read(pipe_in, in_ptr, to_read);

        if (bytes_read == -1) {
            print_error("Could not read form pipe");
            return 5;
        } else if (bytes_read == 0) {
            print_error("Reached pipe EOF before it was expected");
            return 6;
        }

        out_ptr += bytes_read;
        to_read -= bytes_read;
    }
    cout << buf << endl;

    return 0;
}