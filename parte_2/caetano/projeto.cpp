#ifndef PROJETO_HPP
#define PROJETO_HPP

#include "raspberry.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace std;

// Verifica se os nBytes bytes possuem o valor esperado.
bool testaBytes(BYTE *buf, int valor, int nBytes) {
    for (int i = 0; i < nBytes; i++) {
        if (buf[i] != valor)
            return false;
    }
    return true;
}

// Envia todos os bytes, mesmo que send() envie apenas uma parte.
inline void enviaTudo(int sockfd, int nBytes, BYTE *buf) {
    int enviados = 0;

    while (enviados < nBytes) {
        ssize_t n = send(sockfd, buf + enviados,
                         nBytes - enviados, 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            perror("send");
            exit(1);
        }

        if (n == 0) {
            fprintf(stderr, "Conexao encerrada durante send\n");
            exit(1);
        }

        enviados += (int)n;
    }
}

// Recebe todos os bytes solicitados.
inline void recebeTudo(int sockfd, int nBytes, BYTE *buf) {
    int recebidos = 0;

    while (recebidos < nBytes) {
        ssize_t n = recv(sockfd, buf + recebidos,
                         nBytes - recebidos, 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            exit(1);
        }

        if (n == 0) {
            fprintf(stderr, "Conexao encerrada durante recv\n");
            exit(1);
        }

        recebidos += (int)n;
    }
}

class SERVER {
    const string PORT = "3490";
    const int BACKLOG = 1;

    int sockfd;
    int new_fd;
    struct addrinfo hints, *servinfo, *p;

    static void *get_in_addr(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET)
            return &(((struct sockaddr_in *)sa)->sin_addr);

        return &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }

public:
    SERVER() : sockfd(-1), new_fd(-1),
               servinfo(nullptr), p(nullptr) {
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        int rv = getaddrinfo(nullptr, PORT.c_str(),
                             &hints, &servinfo);
        if (rv != 0) {
            fprintf(stderr, "getaddrinfo: %s\n",
                    gai_strerror(rv));
            exit(1);
        }

        for (p = servinfo; p != nullptr; p = p->ai_next) {
            sockfd = socket(p->ai_family, p->ai_socktype,
                            p->ai_protocol);
            if (sockfd == -1) continue;

            int yes = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                       &yes, sizeof yes);

            if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0)
                break;

            close(sockfd);
            sockfd = -1;
        }

        freeaddrinfo(servinfo);
        servinfo = nullptr;

        if (sockfd == -1) {
            perror("server: bind");
            exit(1);
        }

        if (listen(sockfd, BACKLOG) == -1) {
            perror("server: listen");
            close(sockfd);
            exit(1);
        }
    }

    ~SERVER() {
        if (new_fd != -1) close(new_fd);
        if (sockfd != -1) close(sockfd);
    }

    void waitConnection() {
        printf("server: waiting for connections...\n");

        struct sockaddr_storage their_addr;
        socklen_t sin_size = sizeof their_addr;
        char ipstr[INET6_ADDRSTRLEN];

        while (true) {
            new_fd = accept(sockfd,
                (struct sockaddr *)&their_addr, &sin_size);

            if (new_fd == -1) {
                if (errno == EINTR) continue;
                perror("server: accept");
                exit(1);
            }

            inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                ipstr, sizeof ipstr);

            printf("server: got connection from %s\n", ipstr);
            break;
        }
    }

    void sendBytes(int nBytesToSend, BYTE *buf) {
        enviaTudo(new_fd, nBytesToSend, buf);
    }

    void receiveBytes(int nBytesToReceive, BYTE *buf) {
        recebeTudo(new_fd, nBytesToReceive, buf);
    }
};

class CLIENT : public DEVICE {
    const string PORT = "3490";

    int sockfd;
    struct addrinfo hints, *servinfo, *p;

    static void *get_in_addr(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET)
            return &(((struct sockaddr_in *)sa)->sin_addr);

        return &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }

public:
    CLIENT(string endereco) : sockfd(-1),
                              servinfo(nullptr), p(nullptr) {
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int rv = getaddrinfo(endereco.c_str(), PORT.c_str(),
                             &hints, &servinfo);
        if (rv != 0) {
            fprintf(stderr, "getaddrinfo: %s\n",
                    gai_strerror(rv));
            exit(1);
        }

        for (p = servinfo; p != nullptr; p = p->ai_next) {
            sockfd = socket(p->ai_family, p->ai_socktype,
                            p->ai_protocol);
            if (sockfd == -1) continue;

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
                break;

            close(sockfd);
            sockfd = -1;
        }

        freeaddrinfo(servinfo);
        servinfo = nullptr;

        if (sockfd == -1) {
            perror("client: connect");
            exit(1);
        }

        char ipstr[INET6_ADDRSTRLEN];
        inet_ntop(p->ai_family, get_in_addr(p->ai_addr),
                  ipstr, sizeof ipstr);
        printf("client: connected to %s\n", ipstr);
    }

    ~CLIENT() {
        if (sockfd != -1) close(sockfd);
    }

    void sendBytes(int nBytesToSend, BYTE *buf) {
        enviaTudo(sockfd, nBytesToSend, buf);
    }

    void receiveBytes(int nBytesToReceive, BYTE *buf) {
        recebeTudo(sockfd, nBytesToReceive, buf);
    }
};