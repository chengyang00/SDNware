#include "common.hpp"

map<string, map<string, uint64_t>> send_rec;

int read_client_list()
{
    ifstream file("client.txt");
    if (file.is_open())
    {
        string ip;
        while (getline(file, ip))
        {
            send_rec[ip] = map<string, uint64_t>{};
        }
        file.close();
    }
    else
    {
        cerr << "unable to open file." << endl;
        return -1;
    }
    return 0;
}
void init_topo()
{

    // 调用控制器，待完成
    n_sw = 4;
    ip_sw["172.16.1.1"] = 0;
    ip_sw["172.16.2.1"] = 0;
    ip_sw["172.16.3.1"] = 0;
    ip_sw["172.16.4.1"] = 0;
    ip_sw["172.16.5.1"] = 1;
    ip_sw["172.16.6.1"] = 1;
    ip_sw["172.16.7.1"] = 1;
    ip_sw["172.16.8.1"] = 1;
    for (int i = 0; i < n_sw; i++)
    {
        topo.push_back(std::vector<std::vector<int>>());
        for (int j = 0; j < n_sw; j++)
        {
            topo[i].push_back(std::vector<int>());
        }
    }
    topo[0][2].push_back(1);
    topo[0][2].push_back(0);
    topo[0][3].push_back(1);
    topo[0][3].push_back(1);
    topo[1][2].push_back(0);
    topo[1][2].push_back(1);
    topo[1][3].push_back(1);
    topo[1][3].push_back(1);
    topo[2][0].push_back(1);
    topo[2][0].push_back(0);
    topo[3][0].push_back(1);
    topo[3][0].push_back(1);
    topo[2][1].push_back(1);
    topo[2][1].push_back(0);
    topo[3][1].push_back(1);
    topo[3][1].push_back(1);
}
void get_route(std::string ss, std::string dd)
{
    // std::cout << __LINE__ << std::endl;
    std::cout << ss << " " << dd << std::endl;
    int s = ip_sw[ss], d = ip_sw[dd];
    std::cout << s << " " << d << endl;
    int visit[n_sw];
    int pre[n_sw][2];
    cout << __LINE__ << endl;
    for (int i = 0; i < n_sw; i++)
    {
        visit[i] = 0;
        pre[i][0] = 0;
    }
    cout << __LINE__ << endl;
    visit[s] = 1;
    pre[s][0] = -1;
    cout << __LINE__ << endl;
    while (true)
    {
        long long _s = 0, _d = 0, _i = 0, _u = 400000000000;
        for (int i = 0; i < n_sw; i++)
        {
            if (visit[i] == 1)
            {
                for (int j = 0; j < n_sw; j++)
                {
                    if (visit[j] == 0)
                    {
                        for (int k = 0; k < topo[i][j].size(); k++)
                        {
                            if (topo[i][j][k] < _u)
                            {
                                _s = i;
                                _d = j;
                                _i = k;
                                _u = topo[i][j][k];
                            }
                        }
                    }
                }
            }
        }
        cout << __LINE__ << endl;
        visit[_d] = 1;
        pre[_d][0] = _s;
        pre[_d][1] = _i;
        printf("%lld %lld %lld \n", _s, _d, _i);
        cout << __LINE__ << endl;
        if (_d == d)
        {
            int sw = pre[_d][0];
            int idx = pre[_d][1];
            while (sw != -1)
            {
                printf("sip : %s -> dip : %s sw : %d , port : %d \n", ss.c_str(), dd.c_str(), sw, idx);
                idx = pre[sw][1];
                sw = pre[sw][0];
            }
            break;
        }
    }
}
int write_send_record()
{

    std::ofstream outputFile("output.txt");
    // std::cout << __LINE__ << std::endl;
    if (outputFile.is_open())
    {
        for (const auto &outerPair : send_rec)
        {
            // std::cout << __LINE__ << std::endl;
            outputFile << outerPair.first << ":" << endl;
            for (const auto &innerPair : outerPair.second)
            {
                // std::cout << __LINE__ << std::endl;
                get_route(outerPair.first, innerPair.first);
                outputFile << "\t" << innerPair.first << ": " << innerPair.second << endl;
            }

            outputFile << endl;
        }
        outputFile.close();
    }
    else
    {
        cerr << "unable to open file for writing." << endl;
        return -1;
    }
    return 0;
}

// set value of send_rec to zero
void set_rec_zero()
{
    for (auto &outer : send_rec)
        for (auto &inner : outer.second)
            inner.second = 0;
}

void add_send_rec(Amount *amount, string &ip, int len)
{

    for (int j = 0; j < len; j++)
    {

        if (strcmp(amount[j].gid, "\0") == 0)
            break;
        string dip(amount[j].gid);
        if (amount[j].tx > 4096)
        {
            get_route(ip, dip);
        }
        if (amount[j].rx > 4096)
        {
            get_route(dip, ip);
        }
        // if (amount[j].tx + amount[j].rx > 4096)
        cout << amount[j].gid << " " << amount[j].tx << " " << amount[j].rx << " " << amount[j].tm << " " << amount[j].wr_id << endl;
        // send_rec[ip][dip] += amount[j].tx;
        // send_rec[dip][ip] += amount[j].rx;
    }
}

int main()
{
    // std::cout << "start" << endl;
    // std::cout << __LINE__ << std::endl;
    init_topo();
    int ret, sock_fd;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    // std::cout << __LINE__ << std::endl;
    //  read client list
    ret = read_client_list();
    ret = read_server_list();
    if (ret < 0)
    {
        cerr << "cannot open client.txt or server.txt" << endl;
        exit(1);
    }
    int client_num = send_rec.size();
    struct sockaddr_in client_addr[client_num];
    // std::cout << __LINE__ << std::endl;
    //  socket create and verification
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        cerr << "socket creation failed..." << endl;
        exit(1);
    }
    std::cout << __LINE__ << std::endl;
    // assign IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // bind and listen
    if ((bind(sock_fd, (struct sockaddr *)&server_addr, addr_len)) != 0)
    {
        cerr << "socket bind failed..." << endl;
        exit(1);
    }
    listen(sock_fd, client_num);
    // std::cout << __LINE__ << std::endl;
    //  build the connection with clients
    map<int, string> fd_to_ip;
    vector<int> fds;
    for (int i = 0; i < client_num; i++)
    {
        int connect_fd = accept(sock_fd, (struct sockaddr *)&client_addr[i], &addr_len);
        if (connect_fd < 0)
        {
            cerr << "server accept failed..." << endl;
            exit(1);
        }
        int flags;

        if ((flags = fcntl(connect_fd, F_GETFL, 0)) == -1)
            flags = 0;
        fcntl(connect_fd, F_SETFL, flags | O_NONBLOCK);
        fds.push_back(connect_fd);
        fd_to_ip[connect_fd] = inet_ntoa(client_addr[i].sin_addr);
    }
    // std::cout << __LINE__ << std::endl;
    //  receive traffic prediction
    alloc_shared_memory();
    // auto start = chrono::high_resolution_clock::now();
    // chrono::microseconds duration;
    struct timespec timestamp;
    clock_gettime(0, &timestamp);
    long long sec = timestamp.tv_sec;
    long long nsec = timestamp.tv_nsec;
    do
    {
        int size = 0;
        // std::cout << __LINE__ << std::endl;
        // set_rec_zero();
        if ((shm->t) < (shm->h))
        {
            size += ((shm->h) - (shm->t));
            add_send_rec(shm->amount + (shm->t), server_ip, shm->h - shm->t);
            shm->t = shm->h;
        }
        else if (shm->t > shm->h)
        {
            size += flow_NUM - shm->t;
            add_send_rec(shm->amount + (shm->t), server_ip, flow_NUM - (shm->t));
            shm->t = 0;
            if (shm->t < shm->h)
            {
                size += (shm->h - shm->t);
                add_send_rec(shm->amount + (shm->t), server_ip, shm->h - shm->t);
                shm->t = shm->h;
            }
        }
        // if (size)
        //  cout << "size:" << size << " " << shm->t << " " << shm->h << " " << endl;
        for (int i = 0; i < client_num; i++)
        {
            // std::cout << __LINE__ << std::endl;
            Amount *cli_amount = (Amount *)calloc(sizeof(Amount), flow_NUM);
            string ip = fd_to_ip[fds[i]];
            int read_tal = 0;
            read_tal = sock_read(fds[i], (void *)cli_amount, amount_size);
            if (read_tal < 0)
                cerr
                    << "socket read failed..." << endl;
            add_send_rec(cli_amount, ip, read_tal / sizeof(Amount));
            size += read_tal / sizeof(Amount);
            free(cli_amount);
        }
        // if (size)
        //  cout << "size::" << size << endl;

        // clock_gettime(0, &timestamp);
        // while ((timestamp.tv_sec - sec) * 1000000000 + timestamp.tv_nsec - nsec)
        // clock_gettime(0, &timestamp);
        // std::cout << __LINE__ << std::endl;
        // if (size > 0)
        // ret = write_send_record();
        // if (ret < 0)
        // exit(1);
        // auto end = chrono::high_resolution_clock::now();
        // duration = chrono::duration_cast<chrono::microseconds>(end - start);
    } while (true /*duration.count() < TIME*/);
    release_shared_memory();

    // close socket
    close(sock_fd);
    for (const auto &fd : fds)
        close(fd);
    return 0;
}