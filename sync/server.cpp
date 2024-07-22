#include "common.hpp"
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

using namespace utility;              // Common utilities like string conversions
using namespace web;                  // Common features like URIs.
using namespace web::http;            // Common HTTP functionality
using namespace web::http::client;    // HTTP client features
using namespace concurrency::streams; // Asynchronous streams

#define jstring(s) web::json::value::string(s)
using jsvalue = web::json::value;

std::string server_ip = "172.16.50.235";

class Link
{
public:
    std::string port1, port2;
    std::vector<pair<std::string, std::string>> flows;
    long long rate;
    Link(std::string _port1, std::string _port2) : port1(_port1), port2(_port2), rate(0)
    {
    }
};
struct Switch
{
    std::string name, ip;
    int idx;
    Switch(std::string _name, std::string _ip, int _idx) : name(_name), ip(_ip), idx(_idx) {}
};

map<std::string, map<std::string, bool>> route_update;          // 上一次路由更新时间
map<std::string, map<std::string, long long>> time_last_flow;   // 上一条流的到达时间
map<std::string, map<std::string, long long>> time_rate_update; // 上一条流的到达时间
map<std::string, map<std::string, double>> txRates;          // 源目节点之间的传输速率
int n_switch;                                                   // 交换机的数量
web::json::value topo, nodes, linklist;
map<std::string, map<std::string, std::vector<Link>>> links; // 交换机之间的连接

void get_topo()
{
    std::string url = std::string("http://") + server_ip + std::string("/fabric/topo?fabricName=test");
    http_client client(url);
    http_request request(methods::GET);
    request.headers().add(U("token"), U("AD6639F9A63D7D29883F8154184E4DE9"));
    pplx::task<web::http::http_response> response = client.request(request);
    response.wait();
    if (response.get().status_code() == web::http::status_codes::OK)
    {
        web::json::value responseBody = response.get().extract_json().get();
        // std::cout << responseBody["data"][U("deviceList")] << std::endl;
        topo = responseBody["data"];
        nodes = topo["deviceList"];
        linklist = topo["data"]["linkList"];
    }
    else
    {
        std::cout << "request fail" << std::endl;
    }
}

void get_links()
{
    for (int i = 0; i < n_switch; i++)
    {
        for (int j = 0; j < n_switch; j++)
        {
            std::string dev_name_i = nodes.as_array()[i]["deviceName"].as_string();
            std::string dev_name_j = nodes.as_array()[j]["deviceName"].as_string();

            if (links.find(dev_name_i) == links.end())
            {
                links[dev_name_i] = std::map<std::string, std::vector<Link>>();
            }
            if (links.find(dev_name_j) == links.end())
            {
                links[dev_name_j] = std::map<std::string, std::vector<Link>>();
            }
            links[dev_name_i][dev_name_j] = std::vector<Link>();
            links[dev_name_j][dev_name_i] = std::vector<Link>();
        }
    }
    for (auto &l : linklist.as_array())
    {
        std::string dev_name_1 = l["deviceName1"].as_string();
        std::string dev_name_2 = l["deviceName2"].as_string();
        std::string port_1 = l["port1"].as_string();
        std::string port_2 = l["port2"].as_string();
        links[dev_name_1][dev_name_2].emplace_back(port_1, port_2);
        links[dev_name_2][dev_name_1].emplace_back(port_2, port_1);
    }
}
struct server
{
    std::string ip;
    int port;
    long long bw;
    server(std::string _ip,
           int _port,
           long long _bw) : ip(_ip), port(_port), bw(_bw) {}
};

// 从交换机名称转换到交换机索引
int switch_name_to_idx(std::string name)
{
    web::json::array nodes_array = nodes.as_array();
    for (int i = 0; i < nodes_array.size(); i++)
    {
        if (nodes_array[i]["deviceName"] == web::json::value::string(name))
        {
            return i;
        }
    }
    return 0;
}

int switch_ip_to_idx(std::string ip)
{
    web::json::array nodes_array = nodes.as_array();
    for (int i = 0; i < nodes_array.size(); i++)
    {
        if (nodes_array[i]["deviceIp"] == web::json::value::string(ip))
        {
            return i;
        }
    }
    return 0;
}

std::string switch_name_to_ip(std::string name)
{
    return nodes.as_array()[switch_name_to_idx(name)]["deviceIp"].as_string();
}

std::string switch_ip_to_name(std::string ip)
{
    return nodes.as_array()[switch_name_to_idx(ip)]["deviceName"].as_string();
}

jsvalue switch_idx_to_name(int idx)
{
    return nodes.as_array()[idx]["deviceName"];
}

jsvalue switch_idx_to_ip(int idx)
{
    return nodes.as_array()[idx]["deviceIp"];
}

std::map<std::string, std::vector<server>> switch_server_list; // 每个交换机上的服务器列表
std::map<std::string, std::string> server_ip_to_switch_name;   // 从服务器ip到与其连接的交换机的名字

// 读取服务器列表
void get_server_list()
{
    FILE *file_server_list = fopen("server_list.txt", "r");
    char *switch_name;
    while (fscanf(file_server_list, "s", switch_name) == 1)
    {
        switch_server_list[switch_name] = std::vector<server>();
        int n_server;
        fscanf(file_server_list, "%d", &n_server);
        for (int i = 0; i < n_server; i++)
        {
            char *ip;
            int idx_port;
            long long bw;
            fscanf(file_server_list, "%s%d%lld", ip, &idx_port, &bw);
            switch_server_list[switch_name].emplace_back(ip, idx_port, bw);
            server_ip_to_switch_name[ip] = switch_name;
        }
    }
}
web::json::value interfaces;
// 读取端口信息
void get_interfaces()
{
    std::string url = std::string("http://") + server_ip + std::string("/netop/datatable/getRealtimeDataGroupIndexes");

    http_client client(url);
    web::json::value data;

    data[U("tableType")] = web::json::value::string(U("table"));
    data["dataGroupName"] = web::json::value::string("interfaces");
    std::vector<web::json::value> ips;
    for (auto node : nodes.as_array())
    {
        ips.push_back(node["deviceIp"]);
    }
    data[U("deviceIps")] = web::json::value::array(ips);
    web::http::http_request request(web::http::methods::POST);
    request.headers().set_content_type(U("application/json"));
    request.headers().add(U("token"), U("AD6639F9A63D7D29883F8154184E4DE9"));
    request.set_body(data);
    pplx::task<web::http::http_response> response = client.request(request);
    response.wait();
    if (response.get().status_code() == web::http::status_codes::OK)
    {
        web::json::value responseBody = response.get().extract_json().get();
        interfaces = responseBody["data"];
        // std::cout << responseBody["data"]<< std::endl;
    }
    else
    {
        std::cout << "request fail" << std::endl;
    }
}
// 端口索引转端口名
std::string idx_to_ifname(std::string ip, std::string idx)
{
    for (auto interface : interfaces.as_array())
    {
        if (jstring(ip) == interface["value"])
        {
            for (auto inf : interface["children"].as_array())
            {
                if (jstring(idx) == inf["value"])
                {
                    return inf["label"].as_string();
                }
            }
        }
    }
}

// 端口名转端口索引
std::string ifname_to_idx(std::string ip, std::string name)
{
    for (auto interface : interfaces.as_array())
    {
        if (jstring(ip) == interface["value"])
        {
            for (auto inf : interface["children"].as_array())
            {
                if (jstring(name) == inf["label"])
                {
                    return inf["value"].as_string();
                }
            }
        }
    }
}
// 估算传输速率
double get_txRate(std::string sip, std::string dip)
{
    struct timespec timestamp;
    clock_gettime(0, &timestamp);
    double sec = timestamp.tv_sec;
    double nsec = timestamp.tv_nsec;
    return ((1000000000 - (sec * 1000000000 + nsec - time_rate_update[sip][dip])) / 1000000000) * txRates[sip][dip];
}

long long get_interface_rate(int idx1, int idx2, int idx, std::string sip, std::string dip)
{
    std::string dev1 = switch_idx_to_name(idx1).as_string();
    std::string dev2 = switch_idx_to_name(idx2).as_string();
    long long res = 0;
    for (auto flow : links[dev1][dev2][idx].flows)
    {
        if (sip == flow.first && dip == flow.second)
        {
            continue;
        }
        res += txRates[flow.first][flow.second];
    }
    return res;
}

// 计算源目节点之间的最宽路径
void get_route(std::string ss, std::string dd)
{
    int s = switch_name_to_idx(server_ip_to_switch_name[ss]),
        d = switch_name_to_idx(server_ip_to_switch_name[dd]);
    if (s == d)
    {
        return;
    }
    int visit[n_switch];
    int pre[n_switch][2];
    for (int i = 0; i < n_switch; i++)
    {
        visit[i] = 0;
        pre[i][0] = 0;
    }
    visit[s] = 1;
    pre[s][0] = -1;
    while (true)
    {
        long long _s = 0, _d = 0, _i = 0, _u = 400000000000;
        for (int i = 0; i < n_switch; i++)
        {
            if (visit[i] == 1)
            {
                for (int j = 0; j < n_switch; j++)
                {
                    if (visit[j] == 0)
                    {
                        for (int k = 0; k < topo[i][j].size(); k++)
                        {
                            long long rate = get_interface_rate(i, j, k, ss, dd);
                            if (rate < _u)
                            {
                                _s = i;
                                _d = j;
                                _i = k;
                                _u = rate;
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

            double speed = amount[j].tx * 8 * 1000000000 / (amount[j].tm - time_last_flow[ip][dip]);
            struct timespec timestamp;
            clock_gettime(0, &timestamp);
            long long sec = timestamp.tv_sec;
            long long nsec = timestamp.tv_nsec;
            txRates[sip][dip] = (1000000000 -)
                time_last_flow[ip][dip] = amount[j].tm;
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

            long long speed = amount[j].rx * 8 * 1000000000 / (amount[j].tm - time_last_flow[dip][ip]);
            time_last_flow[dip][ip] = amount[j].tm;
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