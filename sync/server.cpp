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
#define win_s 1000000000
std::string sdn_server_ip = "172.16.50.235";
int seq = 1;
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
map<std::string, map<std::string, double>> txRates;             // 源目节点之间的传输速率
int n_switch;                                                   // 交换机的数量
web::json::value topo, nodes, linklist;
map<std::string, map<std::string, std::vector<Link>>> links; // 交换机之间的连接

void get_topo()
{
    std::string url = std::string("http://") + sdn_server_ip + std::string("/fabric/topo?fabricName=test");
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
        linklist = topo["linkList"];
        n_switch = nodes.as_array().size();
        FILE *flie_nodes = fopen("nodes.txt", "w");
        // cout << nodes << endl;
        // cout << linklist << endl;
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
    std::string port;
    long long bw;
    server(std::string _ip,
           std::string _port,
           long long _bw) : ip(_ip), port(_port), bw(_bw) {}
};

struct route
{
    int seq;
    bool del;
    std::string indev, inport, outdev, outport;
    std::vector<pair<std::string, std::string>> pass;
    route() : seq(-1), outport("") {}
};
std::map<std::string, map<string, route>> routes;
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
    char *switch_name = (char *)malloc(32 * sizeof(char));
    while (fscanf(file_server_list, "%s", switch_name) == 1)
    {
        switch_server_list[switch_name] = std::vector<server>();
        int n_server;
        fscanf(file_server_list, "%d", &n_server);
        for (int i = 0; i < n_server; i++)
        {
            char *ip = (char *)malloc(32 * sizeof(char));
            char *idx_port = (char *)malloc(32 * sizeof(char));
            long long bw;
            fscanf(file_server_list, "%s%s%lld", ip, idx_port, &bw);
            switch_server_list[switch_name].emplace_back(ip, idx_port, bw);
            server_ip_to_switch_name[ip] = switch_name;
            free(ip);
            free(idx_port);
        }
    }
    free(switch_name);
}
web::json::value interfaces;
// 读取端口信息
void get_interfaces()
{
    std::string url = std::string("http://") + sdn_server_ip + std::string("/netop/datatable/getRealtimeDataGroupIndexes");

    http_client client(url);
    web::json::value data;

    data["tableType"] = web::json::value::string(U("table"));
    std::vector<web::json::value> ips;
    for (auto node : nodes.as_array())
    {
        ips.push_back(node["deviceIp"]);
    }
    data["deviceIps"] = web::json::value::array(ips);
    data["dataGroupName"] = jstring("interfaces");
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
        // std::cout << responseBody["data"] << std::endl;
        // cout << responseBody << endl;
    }
    else
    {
        std::cout << "request fail" << std::endl;
    }
}
// 端口索引转端口名
std::string idx_to_ifname(std::string ip, std::string idx)
{
    cout << ip << " " << idx << endl;
    for (auto interface : interfaces.as_array())
    {
        cout << interface["value"] << endl;
        if (jstring(ip) == interface["value"])
        {
            for (auto inf : interface["children"].as_array())
            {
                cout << inf["value"] << endl;
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

double get_interface_rate(int idx1, int idx2, int idx, std::string sip, std::string dip)
{
    std::string dev1 = switch_idx_to_name(idx1).as_string();
    std::string dev2 = switch_idx_to_name(idx2).as_string();
    double res = 0;
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
string server_ip_to_if(string ip)
{
    for (auto &sw : switch_server_list)
    {
        for (auto _server : sw.second)
        {
            if (ip == _server.ip)
            {
                return _server.port;
            }
        }
    }
}
void del_route_in_links(std::string sip, std::string dip)
{
    if (routes[sip][dip].pass.size() > 0)
    {
        auto passroute = routes[sip][dip].pass;
        for (auto &_link : links[server_ip_to_switch_name[sip]][passroute[0].first])
        {
            if (_link.port2 == passroute[0].second)
            {
                _link.flows.erase(find(_link.flows.begin(), _link.flows.end(), make_pair(sip, dip)));
            }
        }
        if (passroute.size() > 1)
        {
            for (int i = 0; i < passroute.size() - 1; i++)
            {
                for (auto &_link : links[passroute[i].first][passroute[i + 1].first])
                {
                    if (_link.port2 == routes[sip][dip].pass[0].second)
                    {
                        _link.flows.erase(find(_link.flows.begin(), _link.flows.end(), make_pair(sip, dip)));
                    }
                }
            }
        }
        for (auto &_link : links[passroute[passroute.size() - 1].first][routes[sip][sip].outdev])
        {
            if (_link.port2 == passroute[passroute.size() - 1].second)
            {
                _link.flows.erase(find(_link.flows.begin(), _link.flows.end(), make_pair(sip, dip)));
            }
        }
    }
    else
    {
        for (auto &_link : links[routes[sip][dip].indev][routes[sip][sip].outdev])
        {
            if (_link.port2 == routes[sip][dip].outport)
            {
                _link.flows.erase(find(_link.flows.begin(), _link.flows.end(), make_pair(sip, dip)));
            }
        }
    }
    routes[sip][dip].del = true;
}
void show_links()
{
    for (auto &indev : links)
    {
        for (auto &outdev : indev.second)
        {
            for (auto &_links : outdev.second)
            {
                for (auto &_flow : _links.flows)
                {
                    cout << indev.first << " " << outdev.first << " " << _links.port1 << " " << _links.port2 << " " << _flow.first << " " << _flow.second << endl;
                }
            }
        }
    }
}
void save_route_in_links(std::string sip, std::string dip)
{
    cout << __LINE__ << endl;
    if (routes[sip][dip].pass.size() > 0)
    {
        auto passroute = routes[sip][dip].pass;
        for (auto &_link : links[server_ip_to_switch_name[sip]][passroute[0].first])
        {
            if (_link.port2 == passroute[0].second)
            {
                _link.flows.emplace_back(sip, dip);
            }
        }
        if (passroute.size() > 1)
        {
            for (int i = 0; i < passroute.size() - 1; i++)
            {
                for (auto &_link : links[passroute[i].first][passroute[i + 1].first])
                {
                    if (_link.port2 == routes[sip][dip].pass[0].second)
                    {
                        _link.flows.emplace_back(sip, dip);
                    }
                }
            }
        }
        for (auto &_link : links[passroute[passroute.size() - 1].first][routes[sip][sip].outdev])
        {
            if (_link.port2 == passroute[passroute.size() - 1].second)
            {
                _link.flows.emplace_back(sip, dip);
            }
        }
    }
    else
    {
        for (auto &_link : links[routes[sip][dip].indev][routes[sip][sip].outdev])
        {
            if (_link.port2 == routes[sip][dip].outport)
            {
                _link.flows.emplace_back(sip, dip);
            }
        }
    }
    routes[sip][dip].del = false;
    show_links();
}
void del_route(int seq)
{
    cout << __LINE__ << endl;
    std::string url = std::string("http://") + sdn_server_ip + std::string("/fabric/flowpath/delete");

    http_client client(url);
    web::json::value data;
    data["seq"] = web::json::value::number(seq);
    web::http::http_request request(web::http::methods::DEL);
    request.headers().set_content_type(U("application/json"));
    request.headers().add(U("token"), U("AD6639F9A63D7D29883F8154184E4DE9"));
    request.set_body(data);
    pplx::task<web::http::http_response> response = client.request(request);
    response.wait();
    if (response.get().status_code() == web::http::status_codes::OK)
    {
        web::json::value responseBody = response.get().extract_json().get();
        cout << responseBody << endl;
    }
    else
    {
        std::cout << "request fail" << std::endl;
    }
    cout << __LINE__ << endl;
}
void save_route(std::string sip, std::string dip)
{
    cout << __LINE__ << routes[sip][dip].seq << endl;
    std::string url = std::string("http://") + sdn_server_ip + std::string("/fabric/flowpath/save");

    http_client client(url);
    web::json::value data;

    data["seq"] = jsvalue::number(routes[sip][dip].seq);
    data["fabricName"] = jstring("test");
    data["description"] = jstring("");
    data["inDevice"] = jsvalue();
    data["inDevice"]["deviceAlias"] = jstring(routes[sip][dip].indev);
    data["inDevice"]["portName"] = jstring(routes[sip][dip].inport);
    std::vector<jsvalue> pass;
    for (auto &_pass : routes[sip][dip].pass)
    {
        jsvalue __pass;
        __pass["deviceAlias"] = jstring(_pass.first);
        __pass["portName"] = jstring(_pass.second);
        pass.push_back(__pass);
    }
    data["passDeviceList"] = jsvalue::array(pass);
    data["outDevice"]["deviceAlias"] = jstring(routes[sip][dip].outdev);
    data["outDevice"]["portName"] = jstring(routes[sip][dip].outport);
    std::vector<jsvalue> flow;
    jsvalue _flow;
    _flow["seq"] = jsvalue::number(routes[sip][dip].seq);
    _flow["action"] = jstring("permit");
    _flow["srcIp"] = jstring(sip);
    _flow["dstIp"] = jstring(dip);
    flow.push_back(_flow);
    data["fabricFlowPathRuleEntityList"] = jsvalue::array(flow);
    web::http::http_request request(web::http::methods::POST);
    request.headers().set_content_type(U("application/json"));
    request.headers().add(U("token"), U("AD6639F9A63D7D29883F8154184E4DE9"));
    request.set_body(data);
    pplx::task<web::http::http_response> response = client.request(request);
    response.wait();
    if (response.get().status_code() == web::http::status_codes::OK)
    {
        web::json::value responseBody = response.get().extract_json().get();
        cout << responseBody << endl;
    }
    else
    {
        std::cout << "request fail" << std::endl;
    }
    cout << __LINE__ << endl;
}
// 计算源目节点之间的最宽路径
bool get_route(std::string sip, std::string dip)
{
    cout << __LINE__ << endl;
    int s = switch_name_to_idx(server_ip_to_switch_name[sip]),
        d = switch_name_to_idx(server_ip_to_switch_name[dip]);
    if (s == d)
    {
        return false;
    }
    int visit[n_switch];
    int pre[n_switch][2];
    for (int i = 0; i < n_switch; i++)
    {
        visit[i] = 0;
        pre[i][0] = 0;
        pre[i][1] = 0;
    }
    visit[s] = 1;
    pre[s][0] = s;
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
                        for (int k = 0; k < links[switch_idx_to_name(i).as_string()][switch_idx_to_name(j).as_string()].size(); k++)
                        {
                            long long rate = get_interface_rate(i, j, k, sip, dip);
                            cout << rate << " " << i << " " << j << " " << k << endl;
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
        bool is_update = false;
        if (_d == d)
        {
            cout << "get_route" << endl;
            cout << sip << "->" << dip << endl;
            cout << s << "->" << d << endl;
            for (auto &v : visit)
            {
                cout << v << " ";
            }
            cout << endl;
            for (auto &v : pre)
            {
                cout << v[0] << " " << v[1] << " ";
            }
            cout << endl;
            std::vector<pair<string, string>> tmp_route;
            int sw = d;
            int idx = pre[_d][1];
            while (sw != s)
            {
                string outsw = switch_idx_to_name(sw).as_string(),
                       insw = switch_idx_to_name(pre[sw][0]).as_string();
                tmp_route.emplace_back(outsw, links[insw][outsw][idx].port2);
                printf("sip : %s -> dip : %s sw : %d , port : %d \n", sip.c_str(), dip.c_str(), sw, idx);
                idx = pre[sw][1];
                sw = pre[sw][0];
            }
            cout << __LINE__ << endl;

            reverse(tmp_route.begin(), tmp_route.end());
            if (routes[sip][dip].seq == -1)
            {
                cout << __LINE__ << endl;
                is_update = true;
                routes[sip][dip].seq = seq;
                routes[sip][dip].del = false;
                seq++;
                routes[sip][dip].indev = switch_idx_to_name(s).as_string();
                routes[sip][dip].inport = idx_to_ifname(switch_idx_to_ip(s).as_string(), server_ip_to_if(sip));
                cout << "inport:" << routes[sip][dip].inport << " " << idx_to_ifname(switch_idx_to_ip(s).as_string(), server_ip_to_if(sip)) << endl;
                cout << sip << " " << dip << " " << routes[sip][dip].indev << " " << endl;
                for (int k = 0; k < tmp_route.size() - 1; k++)
                {
                    routes[sip][dip].pass.push_back(tmp_route[k]);
                }
            }
            else if (tmp_route.size() != routes[sip][dip].pass.size() + 1)
            {
                cout << __LINE__ << endl;
                is_update = true;
                if (!routes[sip][dip].del)
                {
                    del_route(routes[sip][dip].seq);
                    del_route_in_links(sip, dip);
                }
                routes[sip][dip].pass.clear();
                for (int k = 0; k < tmp_route.size() - 1; k++)
                {
                    routes[sip][dip].pass.push_back(tmp_route[k]);
                }
            }
            else
            {
                cout << __LINE__ << endl;

                for (int k = 0; k < tmp_route.size() - 1; k++)
                {
                    cout << __LINE__ << " " << k << endl;
                    if (routes[sip][dip].pass[k].first != tmp_route[k].first)
                    {
                        cout << __LINE__ << endl;
                        is_update = true;
                        if (!routes[sip][dip].del)
                        {
                            del_route(routes[sip][dip].seq);
                            del_route_in_links(sip, dip);
                        }
                        routes[sip][dip].pass[k].first = tmp_route[k].first;
                    }
                    if (routes[sip][dip].pass[k].second != tmp_route[k].second)
                    {
                        cout << __LINE__ << endl;
                        is_update = true;
                        if (!routes[sip][dip].del)
                        {
                            del_route(routes[sip][dip].seq);
                            del_route_in_links(sip, dip);
                        }
                        routes[sip][dip].pass[k].second = tmp_route[k].second;
                    }
                }
            }
            routes[sip][dip].outdev = switch_idx_to_name(d).as_string();
            if (routes[sip][dip].outport == "")
            {
                routes[sip][dip].outport = tmp_route[tmp_route.size() - 1].second;
            }
            else if (routes[sip][dip].outport != tmp_route[tmp_route.size() - 1].second)
            {
                cout << __LINE__ << endl;
                cout << routes[sip][dip].outport << " " << tmp_route[tmp_route.size() - 1].second;
                is_update = true;
                if (!routes[sip][dip].del)
                {
                    del_route(routes[sip][dip].seq);
                    del_route_in_links(sip, dip);
                }
                routes[sip][dip].outport = tmp_route[tmp_route.size() - 1].second;
            }
            cout << __LINE__ << endl;
            if (is_update)
            {
                cout << __LINE__ << endl;
                save_route_in_links(sip, dip);
            }
            cout << __LINE__ << endl;

            return is_update;
        }
    }
    return false;
}

bool check_ip(std::string ip)
{
    for (auto &sw : switch_server_list)
    {
        for (auto &_server : sw.second)
        {
            if (_server.ip == ip)
            {
                return true;
            }
        }
    }
    cout << "收到非法ip:" << ip << endl;
    return false;
}
void add_send_rec(Amount *amount, string &sip, int len) // 处理client发送的数据
{
    for (int j = 0; j < len; j++)
    {
        try
        {
            if (strcmp(amount[j].gid, "\0") == 0)
                break;
            string dip(amount[j].gid);
            if (!check_ip(dip))
            {
                continue;
            }
            if (amount[j].tx > 4096)
            {
                long long flow_dt = (amount[j].tm - time_last_flow[sip][dip]);
                if (flow_dt <= 0)
                {
                    continue;
                }
                // assert(flow_dt != 0);
                double speed = amount[j].tx * 8 * 1000000000 / flow_dt;
                time_last_flow[sip][dip] = amount[j].tm;
                struct timespec timestamp;
                clock_gettime(0, &timestamp);
                long long sec = timestamp.tv_sec;
                long long nsec = timestamp.tv_nsec;
                long long now = sec * 1000000000 + nsec;
                long long dt = sec * 1000000000 + nsec - time_rate_update[sip][dip];
                time_rate_update[sip][dip] = now;
                dt = dt > win_s ? win_s : dt;
                txRates[sip][dip] = ((win_s - dt) * txRates[sip][dip] + dt * speed) / win_s;
                // cout << speed << " " << dt << " " << txRates[sip][dip] << endl;
                // cout << "bytes:" << amount[j].tx << endl;
            }
            if (amount[j].rx > 4096)
            {
                long long flow_dt = (amount[j].tm - time_last_flow[dip][sip]);
                if (flow_dt <= 0)
                {
                    continue;
                }
                assert(flow_dt != 0);
                double speed = amount[j].rx * 8 * 1000000000 / (double)(flow_dt);
                time_last_flow[dip][sip] = amount[j].tm;
                struct timespec timestamp;
                clock_gettime(0, &timestamp);
                long long sec = timestamp.tv_sec;
                long long nsec = timestamp.tv_nsec;
                long long now = sec * 1000000000 + nsec;
                long long dt = sec * 1000000000 + nsec - time_rate_update[dip][sip];
                time_rate_update[dip][sip] = now;
                dt = dt > win_s ? win_s : dt;
                txRates[dip][sip] = ((win_s - dt) * txRates[dip][sip] + dt * speed) / win_s;
                // cout << speed << " " << dt << " " << txRates[dip][sip] << endl;
            }
        }
        catch (std::exception &e)
        {
            cout << __LINE__ << " " << e.what() << endl;
        }
    }
}

map<int, string> fd_to_ip;
vector<int> fds;
int sock_fd;

void data_init()
{
    get_topo();
    get_links();
    get_server_list();
    get_interfaces();
}
void show_txRate()
{
    // cout << __LINE__ << endl;
    for (auto &src : txRates)
    {
        // cout << __LINE__ << endl;
        for (auto &dst : src.second)
        {
            // cout << __LINE__ << endl;
            if (dst.second > 0)
            {
                cout << src.first << "->" << dst.first << ":" << dst.second << endl;
            }
        }
    }
}
void update_route()
{
    // cout << __LINE__ << endl;
    for (auto &src : txRates)
    {
        // cout << __LINE__ << endl;
        for (auto &dst : src.second)
        {
            // cout << __LINE__ << endl;
            if (dst.second > 0)
            {
                // cout << __LINE__ << endl;
                if (get_route(src.first, dst.first))
                {
                    cout << "update:" << src.first << "->" << dst.first << ":" << dst.second << endl;
                    save_route(src.first, dst.first);
                }
            }
        }
    }
}
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
    cout << __LINE__ << endl;
    data_init();
    cout << __LINE__ << endl;
    std::thread(connect_client).detach();
    struct timespec timestamp;
    clock_gettime(0, &timestamp);
    long long sec = timestamp.tv_sec;
    long long nsec = timestamp.tv_nsec;
    long long start = sec * 1000000000 + nsec;
    do

    {
        // cout << __LINE__ << endl;
        clock_gettime(0, &timestamp);
        long long now = timestamp.tv_sec * 1000000000 + timestamp.tv_nsec;
        if (start + 1000000000 < now)
        {
            start = now;
            show_txRate();
            update_route();
            cout << __LINE__ << endl;
        }
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