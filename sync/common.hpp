#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <thread>
using namespace std;

/* traffic statistics */
#define KEY_DATA 16789824
#define flow_NUM 10000
#define CACHE_NUM 521
#define PORT 15678
#define MICROSECONDS_PER_SECOND 1000000
#define TIME (30 * MICROSECONDS_PER_SECOND)

typedef struct
{
    char gid[16];
    uint64_t tx;
    uint64_t rx;
    uint64_t wr_id;
    uint64_t tm;
    uint32_t qp_num;
} Amount;

typedef struct
{
    uint32_t qp_num;
    uint8_t path_mtu;
    char gid[16];
} Cache;

typedef struct
{
    pthread_mutex_t lock;
    int t, h;
    Amount amount[flow_NUM];
    Cache cache[CACHE_NUM];
} SHM;

SHM *shm;
int shm_size = sizeof(SHM);
int amount_size = sizeof(Amount) * flow_NUM;
int shmid, shmid_h, shmid_t;
string server_ip;

/* read message from sock_fd */
ssize_t sock_read(int sock_fd, void *buffer, size_t len)
{
    ssize_t nr, tot_read;
    char *buf = (char *)buffer; // avoid pointer arithmetic on void pointer
    tot_read = 0;

    while (len != 0 && (nr = read(sock_fd, buf, len)) != -1)
    {
        if (nr == 0)
        {
            break;
        }
        if (nr <= 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                return -1;
            }
        }
        len -= nr;
        buf += nr;
        tot_read += nr;
    }

    return tot_read;
}

/* write message to sock_fd*/
ssize_t sock_write(int sock_fd, void *buffer, size_t len)
{
    ssize_t nw, tot_written;
    char *buf = (char *)buffer; // avoid pointer arithmetic on void pointer

    for (tot_written = 0; tot_written < len;)
    {
        nw = write(sock_fd, buf, len - tot_written);

        if (nw <= 0)
        {
            if (nw == -1 && errno == EINTR)
            {
                continue;
            }
            else
            {
                return -1;
            }
        }

        tot_written += nw;
        buf += nw;
    }
    return tot_written;
}

int alloc_shared_memory()
{
    shmid = shmget(KEY_DATA, shm_size, IPC_CREAT | 0666);
    cout << shmid << endl;
    if (shmid < 0)
    {
        fprintf(stderr, "Creating shared memory failed\n");
        return -errno;
    }
    shm = (SHM *)shmat(shmid, NULL, 0); // associate shared memory with this process
    bzero(shm, shm_size);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(shm->lock), &attr);
    return 0;
}

/* This function releases shared memory and semaphore */
int release_shared_memory()
{
    shmdt(shm);                    // disassociate shared memory from process
    shmctl(shmid, IPC_RMID, NULL); // delete shared memory
    return 0;
}

void sleep_u(int usec)
{
    struct timespec timestamp;
    clock_gettime(0, &timestamp);
    long long sec = timestamp.tv_sec;
    long long nsec = timestamp.tv_nsec;
    while (true)
    {
        struct timespec timestamp;
        clock_gettime(0, &timestamp);
        long long _sec = timestamp.tv_sec;
        long long _nsec = timestamp.tv_nsec;
        if ((_sec - sec) * 1000000000 + _nsec - nsec > usec * 1000)
        {
            break;
        }
    }
}