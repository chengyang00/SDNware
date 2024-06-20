#include "common.hpp"

// void debug_amount()
// {
//     strcpy(amount[0].gid, "::ffff:172.16.210.35");
//     amount[0].rx = 1000;
//     amount[0].tx = 2000;
// }

// void print_amout()
// {
//     cout << "amount[0].tx: " << amount[0].tx << endl;
//     cout << "amount[0].rx: " << amount[0].rx << endl;
// }

int main(int argc, char **argv)
{
    int sock_fd, option, addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    read_server_list();

    // socket create and verification
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE signal
    if (sock_fd == -1)
    {
        cerr << "socket creation failed..." << endl;
        exit(1);
    }

    // assign IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
    server_addr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sock_fd, (struct sockaddr *)&server_addr, addr_len) != 0)
    {
        cerr << "connection with the server failed..." << endl;
        exit(1);
    }

    alloc_shared_memory();
    // debug_amount();
    int ret = 1;
    while (ret)
    {
        sleep_u(1000);
        pthread_mutex_lock(&(shm->lock));
        if (shm->t < shm->h)
        {
            ret = sock_write(sock_fd, shm->amount + (shm->t), ((shm->h) - (shm->t)) * sizeof(Amount));
            shm->t = shm->h;
        }
        else if (shm->t > shm->h)
        {
            ret = sock_write(sock_fd, shm->amount + (shm->t), (flow_NUM - (shm->t)) * sizeof(Amount));
            shm->t = 0;
            if (shm->t < shm->h)
            {
                ret = sock_write(sock_fd, shm->amount + (shm->t), ((shm->h) - (shm->t)) * sizeof(Amount));
                shm->t = shm->h;
            }
        }
        pthread_mutex_unlock(&(shm->lock));
    }
    release_shared_memory();
    close(sock_fd);
}