#include "common.hpp"

class port // 端口
{
public:
    std::string name;
    long long rate; // 发送速率
    std::string status;
    int idx; // 序号
    port(std::string _name,
         long long _rate,
         std::string _status,
         int _idx) : name(_name), rate(_rate), status(_status), idx(_idx) {}
    port() {}
};

class dev
{
public:
    std::string ip;
    std::string name;
    dev(std::string _ip,
        std::string _name) : ip(_ip), name(_name) {}
};

class route_node
{
public:
    std::string name;
    std::string port;
    route_node *next;
};
class route
{
    int seq;
    route_node *first;
};

map<std::string, int> ip_sw; // 服务器ip映射到交换机序号
int n_sw;                    // 交换机数量
std::string cur_net_name;    // 网络名称
std::vector<dev> dev_list    /*设备列表*/
    = {dev("172.16.50.231", "leaf1"), dev("172.15.50.231", "leaf2"), dev("172.16.50.233", "spain1"), dev("172.16.50.234", "spain2")};
std::map<std::string, int> dev_name_to_idx;                 // 设备名映射序号
std::vector<std::vector<std::vector<port>>> topo;           // 拓扑
std::map<std::string, std::map<std::string, route>> routes; // 路由集合
std::string get_network()
{
    // todo
    return "test";
}

void init_topo()
{
    n_sw = 4;
    ip_sw["172.16.1.1"] = 0;
    ip_sw["172.16.2.1"] = 0;
    ip_sw["172.16.3.1"] = 0;
    ip_sw["172.16.4.1"] = 0;
    ip_sw["172.16.5.1"] = 1;
    ip_sw["172.16.6.1"] = 1;
    ip_sw["172.16.7.1"] = 1;
    ip_sw["172.16.8.1"] = 1;
    cur_net_name = get_network();
    int n_dev = 0;
    for (auto dev : dev_list)
    {
        dev_name_to_idx[dev.name] = n_dev;
        topo.emplace_back();
        n_dev += 1;
    }
    for (auto &t : topo)
    {

        t.emplace_back();
        t.emplace_back();
        t.emplace_back();
        t.emplace_back();
    }
    topo[0][2].push_back(port(std::string("eth-0-5"), 1, std::string("up"), 5));
    topo[0][2].push_back(port(std::string("eth-0-6"), 0, std::string("up"), 6));
    topo[0][3].push_back(port(std::string("eth-0-7"), 1, std::string("up"), 7));
    topo[0][3].push_back(port(std::string("eth-0-8"), 1, std::string("up"), 8));
    topo[1][2].push_back(port(std::string("eth-0-5"), 1, std::string("up"), 5));
    topo[1][2].push_back(port(std::string("eth-0-6"), 1, std::string("up"), 6));
    topo[1][3].push_back(port(std::string("eth-0-7"), 0, std::string("up"), 7));
    topo[1][3].push_back(port(std::string("eth-0-8"), 1, std::string("up"), 8));
    topo[2][0].push_back(port(std::string("eth-0-1"), 1, std::string("up"), 1));
    topo[2][0].push_back(port(std::string("eth-0-2"), 1, std::string("up"), 2));
    topo[2][1].push_back(port(std::string("eth-0-3"), 0, std::string("up"), 3));
    topo[2][1].push_back(port(std::string("eth-0-4"), 1, std::string("up"), 4));
    topo[3][0].push_back(port(std::string("eth-0-1"), 1, std::string("up"), 1));
    topo[3][0].push_back(port(std::string("eth-0-2"), 0, std::string("up"), 2));
    topo[3][1].push_back(port(std::string("eth-0-3"), 1, std::string("up"), 3));
    topo[3][1].push_back(port(std::string("eth-0-4"), 1, std::string("up"), 4));
}
void get_port_idx_from_ip(std::string ip)
{
    // int ip_idx =
}
void init_cur_net(void);
map<string, map<string, uint64_t>> send_rec;
map<std::string, map<std::string, bool>> route_update;   // 上一次路由更新时间
map<std::string, map<std::string, long long>> last_flow; // 上一条流的到达时间

void get_route(std::string ss, std::string dd) // 计算源目节点之间的最宽路径
{
    int s = ip_sw[ss], d = ip_sw[dd];
    int visit[n_sw];
    int pre[n_sw][2];
    for (int i = 0; i < n_sw; i++)
    {
        visit[i] = 0;
        pre[i][0] = 0;
    }
    visit[s] = 1;
    pre[s][0] = -1;
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
                            if (topo[i][j][k].rate < _u)
                            {
                                _s = i;
                                _d = j;
                                _i = k;
                                _u = topo[i][j][k].rate;
                            }
                        }
                    }
                }
            }
        }
        visit[_d] = 1;
        pre[_d][0] = _s;
        pre[_d][1] = _i;
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

bool get_route_update(std::string sip, std::string dip) // 查看源目节点之间是否可以更新路由
{
    if (route_update.find(sip) != route_update.end())
    {
        if (route_update[sip].find(dip) != route_update[sip].end())
        {
            return route_update[sip][dip];
        }
        else
        {
            route_update[sip][dip] = false;
        }
    }
    else
    {
        route_update[sip] = std::map<std::string, bool>();
        route_update[sip][dip] = false;
    }
    return route_update[sip][dip];
}
void add_send_rec(Amount *amount, string &ip, int len) // 处理client发送的数据
{
    for (int j = 0; j < len; j++)
    {
        if (strcmp(amount[j].gid, "\0") == 0)
            break;
        string dip(amount[j].gid);
        if (amount[j].tx > 4096)
        {

            double speed = amount[j].tx * 8 * 1000000000 / (amount[j].tm - last_flow[ip][dip]);
            last_flow[ip][dip] = amount[j].tm;
            if (speed > 1000000000 && !get_route_update(ip, dip))
            {
                route_update[ip][dip] = true;
                get_route(ip, dip);
            }
            else if (speed < 1000000000 && get_route_update(ip, dip))
            {
                route_update[ip][dip] = false;
            }
        }
        if (amount[j].rx > 4096)
        {

            long long speed = amount[j].rx * 8 * 1000000000 / (amount[j].tm - last_flow[dip][ip]);
            last_flow[dip][ip] = amount[j].tm;
            if (speed > 1000000000 && !get_route_update(dip, ip))
            {
                route_update[dip][ip] = true;
                get_route(dip, ip);
            }
            else if (speed < 1000000000 && get_route_update(dip, ip))
            {
                route_update[dip][ip] = false;
            }
        }
    }
}

map<int, string> fd_to_ip;
vector<int> fds;
int sock_fd;

void connect_client()
{
    int ret;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        cerr << "cannot open client.txt or server.txt" << endl;
        exit(1);
    }
    struct sockaddr_in client_addr;
    //  socket create and verification
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        cerr << "socket creation failed..." << endl;
        exit(1);
    }
    // assign IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    if ((bind(sock_fd, (struct sockaddr *)&server_addr, addr_len)) != 0)
    {
        cerr << "socket bind failed..." << endl;
        exit(1);
    }
    listen(sock_fd, 1024);
    //  build the connection with clients
    while (true)
    {
        int connect_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addr_len);
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
        fd_to_ip[connect_fd] = inet_ntoa(client_addr.sin_addr);
        cout << "connect:" << fd_to_ip[connect_fd] << endl;
    }
}

int main()
{
    init_topo();
    std::thread(connect_client).detach();
    do
    {
        for (int i = 0; i < fds.size(); i++)
        {
            Amount *cli_amount = (Amount *)calloc(sizeof(Amount), flow_NUM);
            string ip = fd_to_ip[fds[i]];
            int read_tal = 0;
            read_tal = sock_read(fds[i], (void *)cli_amount, amount_size);
            if (read_tal < 0)
                cerr
                    << "socket read failed..." << endl;
            add_send_rec(cli_amount, ip, read_tal / sizeof(Amount));
            free(cli_amount);
        }
    } while (true);
    // close socket
    close(sock_fd);
    for (const auto &fd : fds)
        close(fd);
    return 0;
}