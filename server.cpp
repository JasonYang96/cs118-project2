#include "packet.h"
#include <cstring> // for memset
#include <iostream> // for cout
#include <stdio.h> // for perror
#include <string> // for string
#include <sys/socket.h> // for socket
#include <sys/types.h> // for types
#include <netdb.h> // for getaddrinfo
#include <sys/stat.h> // for open
#include <fcntl.h> // for open
#include <unistd.h> // for close, read

using namespace std;

void process_error(int status, const string &function);
int open_file(char* file);
int set_up_socket(char* port);

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cout << "Usage: " << argv[0] << " PORT-NUMBER FILE-NAME" << endl;
        exit(1);
    }

    int file_fd = open_file(argv[2]);
    int sockfd = set_up_socket(argv[1]);

    struct sockaddr_storage recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    int status, n_bytes;
    uint16_t ack_num;
    Packet p;

    // select random seq_num
    srand(time(NULL));
    uint16_t seq_num = rand() % MSN;

    // recv SYN from client
    do
    {
        n_bytes = recvfrom(sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr *) &recv_addr, &addr_len);
        process_error(n_bytes, "recv SYN");
    } while (!p.syn_set());
    cout << "Receiving syn packet with seq " << p.seq_num() << endl;
    ack_num = p.seq_num() + 1;

    // sending SYN ACK
    p = Packet(1, 1, 0, seq_num, ack_num, 0, "");
    status = sendto(sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr *) &recv_addr, addr_len);
    process_error(status, "sending SYN ACK");
    cout << "Sending data packet with seq " << seq_num << endl;
    seq_num += 1;

    // recv ACK
    n_bytes = recvfrom(sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr *) &recv_addr, &addr_len);
    process_error(n_bytes, "recv ACK after SYN ACK");
    cout << "Receiving ACK packet with ack " << p.ack_num() << endl;
    ack_num = p.seq_num() + 1;


    // send file
    do
    {
        // split file into sections
        string data;
        size_t buf_pos = 0;
        data.resize(DATA_LENGTH - 1);
        do
        {
            n_bytes = read(file_fd, &data[buf_pos], DATA_LENGTH - 1);
            buf_pos += n_bytes;
        } while (n_bytes != 0 && buf_pos != DATA_LENGTH - 1);

        // send data packet
        p = Packet(0,0,0, seq_num, ack_num, buf_pos, data.c_str());
        status = sendto(sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr *) &recv_addr, addr_len);
        process_error(status, "sending packet");
        cout << "Sending data packet with seq " << seq_num << endl;
        seq_num += p.data_len();
        cout << "sending packet of size " << sizeof(p) << " with size " << p.data_len() << " and " << p.data().size() << endl;

        // recv ACK
        int ack_n_bytes = recvfrom(sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr *) &recv_addr, &addr_len);
        process_error(ack_n_bytes, "recv ACK after SYN ACK");
        cout << "Receiving ACK packet with ack " << p.ack_num() << endl;
        ack_num = p.seq_num() + 1;
    } while (n_bytes != 0);

    // send FIN
    p = Packet(0,0,1, seq_num, ack_num, 0, "");
    status = sendto(sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr *) &recv_addr, addr_len);
    process_error(status, "sending FIN");
    cout << "Sending data packet with seq " << seq_num << endl;
    seq_num += 1;

    // recv FIN ACK
    do
    {
        n_bytes = recvfrom(sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr *) &recv_addr, &addr_len);
        process_error(n_bytes, "recv FIN ACK");
        cout << "Receiving ACK packet with ack " << p.ack_num() << endl;
        ack_num = p.seq_num() + 1;
    } while (!p.fin_set() || !p.ack_set());

    // send ACK after FIN ACK
    p = Packet(0,1,0, seq_num, ack_num, 0, "");
    status = sendto(sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr *) &recv_addr, addr_len);
    process_error(status, "sending ACK after FIN ACK");
    cout << "Sending data packet with seq " << seq_num << endl;
    seq_num += 1;
}

int open_file(char* file)
{
    // tries to open file served
    int file_fd = open(file, O_RDONLY);
    if (file_fd < 0)
    {
        cerr << "Could not open " << file << endl;
        exit(1);
    }

    // makes sure it's a regular file
    struct stat buf;
    int status = fstat(file_fd, &buf);
    if (status == -1)
    {
        perror("fstat");
        close(file_fd);
        exit(1);
    }

    if (!S_ISREG(buf.st_mode))
    {
        cerr << file << " is not a regular file" << endl;
        exit(1);
    }

    // TODO: FOR DEBUGGING PURPOSES ONLY, GET RID WHEN DONE
    cout << "file is " << buf.st_size << " bytes" << endl;

    return file_fd;
}

int set_up_socket(char* port)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int status;

    // set up hints addrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    //set up socket calls
    status = getaddrinfo(NULL, port, &hints, &res);
    if (status != 0)
    {
        cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
        exit(1);
    }

    // find socket to bind to
    int sockfd;
    int yes = 1;
    auto i = res;
    for (; i != NULL; i = i ->ai_next)
    {
        sockfd = socket(res->ai_family, res->ai_socktype, 0);
        if (sockfd == -1) 
        {
            continue;
        }

        status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (status == -1)
        {
            continue;
        }

        status = bind(sockfd, res->ai_addr, res->ai_addrlen);
        if (status == -1)
        {
            continue;
        }

        break;
    }
    freeaddrinfo(res);

    // check if reached end of linked list
    // means could not find a socket to bind to
    if (i == NULL)
    {
        perror("bind to a socket");
        exit(1);
    }

    return sockfd;
}

void process_error(int status, const string &function)
{
    if (status == -1)
    {
        perror(&function[0]);
        exit(1);
    }
}
