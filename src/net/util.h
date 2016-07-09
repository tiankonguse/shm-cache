#ifndef __UTIL_H__
#define __UTIL_H__
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <error.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

class LockHelper
{
public:
    explicit LockHelper(pthread_mutex_t *lock){
        pthread_mutex_lock(lock);
        _lock = lock;
    }

    ~LockHelper(){
        pthread_mutex_unlock(_lock);
    }

private:
    pthread_mutex_t *_lock;
};

class StringTokenizer: public std::vector<std::string>{
public:
    StringTokenizer(const std::string &str, const std::string &sep){
        std::string::const_iterator n = str.begin(), b, e, end = str.end();
        while (n != end) {
            while (n != end && isspace(*n))
                ++n;
            b = n;
            while (b != end && sep.find(*b) == std::string::npos)
                ++b;
            e = b;
            if (e != n) {
                --e;
                while (e != n && isspace(*e))
                    --e;
                if (!isspace(*e))
                    ++e;
            }

            if (e != n)
                this->push_back(std::string(n, e));
            n = b;
            if (n != end)
                ++n;
        }
    }
};

class Clock {
public:
    static uint64_t rdtsc(){
        uint32_t low = 0, high = 0;
        __asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
        return (uint64_t) high << 32 | low;
    }

    static uint64_t NowWithUs(){
        struct timeval tv;
        int rc = gettimeofday (&tv, NULL);
        assert(rc == 0);

        return tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec;
    }
};

static inline bool is_innerip(unsigned int ip){
    return (ip >= 0x0A000000 && ip < 0x0B000000) || // [10.0.0.0, 11.0.0.0)
           (ip >= 0xAC000000 && ip < 0xAD000000) || // [172.0.0.0, 173.0.0.0)
           (ip >= 0xC0000000 && ip < 0xC1000000) || // [192.0.0.0, 193.0.0.0)
           (ip == 0x7F000001);                      // 127.0.0.1
}

static inline int get_hostip(const std::string &interface, std::string &ipaddr){
    struct sockaddr_in sin;
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    strncpy(ifr.ifr_name, interface.c_str(), 10);
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
        return -1;

    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    ipaddr = inet_ntoa(sin.sin_addr);
    close(sock);
    return 0;
}

static inline unsigned get_hostip(std::string *ipaddr=NULL){
    char addr[64] = { 0 };
    struct sockaddr_in sin;
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        for (int i = 0; i < 5; i++) {
            snprintf(ifr.ifr_name, 10, "eth%d", i);
            if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
                continue;
            memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
            snprintf(addr, sizeof(addr), "%s", inet_ntoa(sin.sin_addr));

            unsigned ip = inet_addr(addr);
            if (is_innerip(ntohl(ip))) {
                close(sock);
                if (ipaddr)
                    *ipaddr = addr;
                return ip;
            }
        }

        close(sock);
    }

    return -1;
}

// 测试一个fd[]是否可读
static inline bool IsReadable(int fd[], size_t n, size_t timeout){
    struct pollfd fds[n];
    for (size_t i = 0; i < n; i++) {
        fds[i].fd = fd[i];
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    return poll(fds, n, timeout);
}

static inline int TcpConnect(const char *ip, int port)
{
    int s = 0, on = 1, flags = 0;
    struct sockaddr_in sa;

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_aton(ip, &(sa.sin_addr)) == 0)
        return -1;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        goto ERR;

    if ((flags = fcntl(s, F_GETFL)) < 0 || fcntl(s, F_SETFL, flags|O_NONBLOCK) < 0)
        goto ERR;

    if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        if (errno != EINPROGRESS)
            goto ERR;
    }

    return s;
ERR:
    if (s >= 0)
        close(s);
    return -1;
}

static inline bool SocketIsAlive(int fd){
    if (fd < 0)
        return false;
    char buff[1];
    int len = recv(fd, buff, 1, MSG_PEEK);
    if (len == 0)
        return false;
    if (len > 0 || errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
        return true;
    return false;
}


class PipeEvent
{
public:
    PipeEvent()
    {
        int rc = pipe(pipefd);
        assert(rc == 0);
        rc = SetNonBlock(pipefd[0]);
        assert(rc == 0);
        rc = SetNonBlock(pipefd[1]);
        assert(rc == 0);
    }

    ~PipeEvent()
    {
        if (pipefd[0] >= 0) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
    }

    int SendEvent()
    {
        char cmd = 0;
        return write(pipefd[1], &cmd, 1) == 1 ? 0 : -1;
    }

    void ReadAll()
    {
        char buff[32];
        int rc = 0;
        do {
            rc = read(pipefd[0], buff, 32);
        } while (rc > 0);
    }

    int GetFD()
    {
        return pipefd[0];
    }

private:
    static int SetNonBlock(int fd)
    {
        int flags;
        if ((flags = fcntl(fd, F_GETFL)) == -1)
            return -1;
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            return -1;
        return 0;
    }

private:
    int pipefd[2];
};

struct NetBuff
{
    NetBuff()
    {
        buffer = NULL;
        capacity = 0;
        size = 0;
    }

    ~NetBuff()
    {
        if (buffer)
            free(buffer);
    }

    int Expand(size_t len)
    {
        if (len <= capacity)
            return 0;
        char *ptr = (char *)realloc(buffer, len);
        if (ptr == NULL)
            return -1;
        buffer = ptr, capacity = len;

        return 0;
    }

    void Clean()
    {
        size = 0;
    }

    int Append(const char *ptr, size_t len)
    {
        if (len + size > capacity) {
            size_t n = capacity * 2;
            while (n < len + size)
                n *= 2;

            if (Expand(n) < 0)
                return -1;
        }

        memcpy(buffer + size, ptr, len);
        size += len;

        return 0;
    }

    int Reserve(size_t len)
    {
        if (capacity - size >= len)
            return 0;
        size_t n = capacity ? capacity * 2 : 128;

        while (n < len + size)
            n *= 2;

        return Expand(n);
    }

    char    *buffer;
    unsigned capacity;
    unsigned size;
};

#endif // __UTIL_H__

