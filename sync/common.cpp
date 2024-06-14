#include "common.hpp"
void init_cur_net()
{
    cur_net.name = "test";
    cur_net.dev_list.push_back({"sw1", "ip"});
    cur_net.dev_list.push_back({"sw2", "ip"});
    cur_net.dev_list.push_back({"sw3", "ip"});
    cur_net.dev_list.push_back({"sw4", "ip"});
    cur_net.dev_list[0].nrb_port_list.push_back(std::vector<port>());
    cur_net.dev_list[0].nrb_port_list.push_back(std::vector<port>());
    cur_net.dev_list[1].nrb_port_list.push_back(std::vector<port>());
    cur_net.dev_list[1].nrb_port_list.push_back(std::vector<port>());
    cur_net.dev_list[2].nrb_port_list.push_back(std::vector<port>());
    cur_net.dev_list[2].nrb_port_list.push_back(std::vector<port>());
    cur_net.dev_list[3].nrb_port_list.push_back(std::vector<port>());
    cur_net.dev_list[3].nrb_port_list.push_back(std::vector<port>());
    cur_net.dev_list[0].nrb_port_list[0].push_back({"eth-0-1", 0, 0, 2, 0, 0, ""});
}