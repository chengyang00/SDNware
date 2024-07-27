
from copy import deepcopy
import random
import requests
import json
import routesolve


token = "AD6639F9A63D7D29883F8154184E4DE9"
ip = "172.16.50.235"


def get_topo(ip):
    header = {"token": token}
    url = "http://"+ip+"/fabric/topo?fabricName=test"
    res = requests.get(url, headers=header)
    res_json = res.json()
    if res_json['code'] != 0:
        print("get topo fail. code:", res_json['code'])
    return res_json


topo = get_topo(ip)
n_node = len(topo["data"]["deviceList"])
nodes = topo["data"]["deviceList"]


def ip_to_name(ip):
    for node in nodes:
        if ip == node["deviceIp"]:
            return node["deviceName"]


def name_to_ip(name):
    for node in nodes:
        if name == node["deviceName"]:
            return node["deviceIp"]
    # return name


def name_to_idx(name):
    for i in range(len(nodes)):
        if name == nodes[i]["deviceName"]:
            return i


def idx_to_name(idx):
    return nodes[idx]["deviceName"]


linklist = topo["data"]["linkList"]

links = {}


for node in nodes:
    for _node in nodes:
        if node["deviceName"] not in links:
            links[node["deviceName"]] = {}
        if _node["deviceName"] not in links:
            links[_node["deviceName"]] = {}
        links[node["deviceName"]][_node["deviceName"]] = []
        links[_node["deviceName"]][node["deviceName"]] = []

for l in linklist:
    links[l["deviceName1"]][l["deviceName2"]].append([l["port1"], l["port2"]])
    links[l["deviceName2"]][l["deviceName1"]].append([l["port2"], l["port1"]])


def get_if(ip):
    header = {"token": token}
    url = "http://"+ip+"/netop/datatable/getRealtimeDataGroupIndexes"
    data = {}
    data["tableType"] = "table"
    data["dataGroupName"] = "interfaces"
    data["deviceIps"] = [i["deviceIp"] for i in nodes]
    res = requests.post(url, json=data, headers=header)
    res_json = res.json()
    return res_json


interfaces = get_if(ip)["data"]


def idx_to_if(ip, idx):
    assert ip != None
    ip = name_to_ip(ip)
    for interface in interfaces:
        if ip == interface['value']:
            for inf in interface["children"]:
                if str(idx) == inf["value"]:
                    inf["label"] = "E"+inf["label"][1:]
                    return inf["label"]
    print("noipidx", ip, idx)


def if_to_idx(ip, ifname):
    for interface in interfaces:
        _ip = name_to_ip(ip)
        if _ip == interface['value']:
            for inf in interface["children"]:
                if ifname[1:] == inf["label"][1:]:
                    return inf["value"]
    # print("novalue")


def get_data_table(ip):
    header = {"token": token}
    url = "http://"+ip+"/netop/datatable/getRealtimeData"
    data = {}
    data["tableType"] = "table"
    data["dataGroupName"] = "interfaces"
    data["deviceIps"] = [i["deviceIp"] for i in nodes]
    data["dataItemNames"] = ["ifOutRate", "ifHighSpeed", "ifInRate"]
    data["deviceIndexMap"] = {}
    for node in nodes:
        data_set = set()
        for link in linklist:
            if link["deviceName1"] == node["deviceName"]:
                data_set.add(if_to_idx(link["deviceName1"], link['port1']))
            if link["deviceName2"] == node["deviceName"]:
                data_set.add(if_to_idx(link["deviceName2"], link['port2']))
        data["deviceIndexMap"][name_to_ip(node["deviceName"])] = list(data_set)
    json_data = json.dumps(data)
    res = requests.post(url, json=data, headers=header)
    res_json = res.json()
    return res_json


if_table = get_data_table(ip)
iflist = if_table["data"]["snmpDataList"]


for interface in iflist:
    # print(interface)
    for l in links:
        # print(l, ip_to_name(interface["ip"]))
        if l == ip_to_name(interface["ip"]):
            print(l, interface["ip"], ip_to_name(interface["ip"]))
            for ll in links[l]:
                for lll in links[l][ll]:
                    # print(lll[0], idx_to_if(l, interface["ifIndex"]),
                    #     l, interface["ifIndex"])
                    # topo返回的是Eth，别的地方却是eth
                    if lll[0][1:] == idx_to_if(l, interface["ifIndex"])[1:]:
                        # print(l, ll, lll, interface["ifHighSpeed"])
                        lll.append(interface["ifHighSpeed"]/1000)

print(links)


def topo_to_file():
    f = open("topo.txt", "w")
    file_switch = open("switch.txt", "w")
    idx_topo = {}
    for i in range(n_node):
        name = nodes[i]["deviceName"]
        file_switch.write(nodes[i]["deviceName"]+"\n")
        idx_topo[i] = {}
        for j in range(n_node):
            idx_topo[i][j] = 0
    for l in links:
        for ll in links[l]:
            for lll in links[l][ll]:
                idx_topo[name_to_idx(l)][name_to_idx(ll)] += lll[2]
    for u in idx_topo:
        for v in idx_topo[u]:
            if idx_topo[u][v] != 0:
                f.write("{} {} {}\n".format(u, v, idx_topo[u][v]))


def save_route(ip, route):
    header = {"token": token}
    url = "http://"+ip+"/fabric/flowpath/save"
    res = requests.post(url, json=route, headers=header)
    res_json = res.json()
    # print(res_json)
    return res_json


def del_route(ip, seq):
    header = {"token": token}
    url = "http://"+ip+"/fabric/flowpath/delete"
    data = {"seq": seq}
    res = requests.delete(url, json=data, headers=header)
    res_json = res.json()
    # print(res_json, seq)
    return res_json


def get_route(ip):
    header = {"token": token}
    url = "http://"+ip+"/fabric/flowpath/list?fabricName=sl-sk"
    res = requests.post(url, headers=header)
    res_json = res.json()
    # print(res_json)
    return res_json


get_route(ip)

for i in range(1, 9):
    # del_route(ip, i)
    pass

topo_to_file()

route = routesolve.lp_mlu_route(n_node)
route.get_routing()
route.main()


class nonblocking:
    def __init__(self) -> None:
        self.n_node = n_node
        self.switch = route.bw_host
        self.switch_out = [0 for i in range(self.n_node)]  # 交换机出宽带使用量
        self.switch_in = [0 for i in range(self.n_node)]  # 交换机入宽带使用量
        self.capacity = route.capacity  # 链路容量
        self.link_usage = [
            [0 for i in range(self.n_node)] for j in range(self.n_node)]  # 链路使用量
        self.routing = {}  # 路由
        self.routing_update = [
            [0 for i in range(self.n_node)] for j in range(self.n_node)]
        self.server = [[] for i in range(self.n_node)]

        """路由录入"""

        for u in range(self.n_node):
            if self.switch[u] == 0:
                continue
            self.routing[u] = {}
            for v in range(self.n_node):
                if self.switch[v] == 0:
                    continue
                if u == v:
                    continue
                self.routing[u][v] = {}
                for i in range(self.n_node):
                    self.routing[u][v][i] = {}
                    for j in range(self.n_node):
                        if i == j:
                            continue
                        if self.capacity[i][j] == 0:
                            continue
                        self.routing[u][v][i][j] = 0
        f = open("route.txt", 'r')
        s = f.readline()
        while s != "":
            ss = s.split(" ")
            u, v, i, j, fuvij = int(float(ss[0])), int(float(ss[1])), int(
                float(ss[2])), int(float(ss[3])), float(ss[4])
            self.routing[u][v][i][j] = fuvij
            s = f.readline()

        self.new_routing = deepcopy(self.routing)
        self._routing = deepcopy(self.new_routing)
        file_server = open("server.txt", "r")
        s = file_server.readline()
        while s != "":
            idx = name_to_idx(s[:-1])
            n_server = int(float(file_server.readline()[:-1]))
            for i in range(n_server):
                s = file_server.readline()
                ss = s.split(" ")
                self.server[idx].append(
                    [ss[0], int(float(ss[1])), float(ss[2])])
            s = file_server.readline()

    def link_in_path(self, i, j, path):
        k = 0
        while k < len(path)-1:
            if path[k] == i and path[k+1] == j:
                return True
            k += 1
        return False

    def get_paths(self, u, v, i, f, vist):  # 计算u->v的所有路径
        _paths = []  # 路径集合
        for j in range(self.n_node):
            if vist[j]:
                continue
            if j == i or self.capacity[i][j] == 0:
                continue
            if self._routing[u][v][i][j] <= 1e-5:
                continue
            _f = min(f, self._routing[u][v][i][j])
            f -= _f
            self._routing[u][v][i][j] -= _f
            assert self._routing[u][v][i][j] >= 0
            if j == v:
                _paths.append([[i, j], _f])
            else:
                vist[j] = 1
                paths = self.get_paths(u, v, j, _f, vist)
                vist[j] = 0
                for path_and_f in paths:
                    _paths.append([[i, *(path_and_f[0])], path_and_f[1]])
            if f < 1e-5:
                break
        return _paths

    def get_all_paths(self):
        self._routing = deepcopy(self.new_routing)
        # self.show__routing()
        # print(" ")
        paths = {}
        for u in range(self.n_node):
            if self.switch[u] == 0:
                continue
            paths[u] = {}
            for v in range(self.n_node):
                if self.switch[v] == 0:
                    continue
                if u == v:
                    continue
                """if self.traffic[u][v]==0:
                    pass"""
                paths[u][v] = self.get_paths(
                    u, v, u, 1, [0 for i in range(self.n_node)])
                # print(u, v, paths[u][v])
        return paths

    def get_s_to_d_routing(self):  # 计算源目节点之间的路由
        seq_p = 128
        seq_f = 128
        paths = self.get_all_paths()
        # print(paths)
        for u in range(self.n_node):  # 从交换机u开始
            if self.switch[u] == 0:
                continue
            for u_i in range(len(self.server[u])):  # 交换机u下的d第i个服务器
                for v in range(self.n_node):  # 到交换机v
                    if self.switch[v] == 0:
                        continue
                    if u == v:
                        continue
                    # 寻找一个最宽的路径
                    idx = 0
                    bw = 0
                    for i in range(len(paths[u][v])):
                        if paths[u][v][i][1] > bw:
                            idx = i
                            bw = paths[u][v][i][1]
                    p = paths[u][v][idx][0]
                    i = 0
                    # print("path:bw:", bw, paths[u][v][idx])
                    # print("links2-2",links["Leaf2"]["Spine2"],idx,paths[u][v])
                    route_u_i_v = [[u, u_i, v]]
                    while i < len(p)-1:
                        s = p[i]
                        d = p[i+1]
                        idx_p = 0
                        j = 0
                        bw = 0

                        while j < len(links[idx_to_name(s)][idx_to_name(d)]):
                            # print(links[idx_to_name(s)][idx_to_name(d)][j][2],bw)
                            if links[idx_to_name(s)][idx_to_name(d)][j][2] > bw:
                                idx_p = j
                                bw = links[idx_to_name(
                                    s)][idx_to_name(d)][j][2]
                            j += 1
                        # print(bw,idx_p,s,d)
                        if idx < len(links[idx_to_name(s)][idx_to_name(d)]):
                            links[idx_to_name(s)][idx_to_name(
                                d)][idx_p][2] -= self.server[u][u_i][2]
                            route_u_i_v.append([s, idx_p, d])
                        else:
                            print("no")
                            print(links[idx_to_name(s)][idx_to_name(d)])
                            route_u_i_v.append(
                                [s, random.randint(0, len(links[idx_to_name(s)][idx_to_name(d)])-1), d])
                        i += 1
                    paths[u][v][idx][1] -= 1/len(self.server[u])
                    print(route_u_i_v)
                    flow_route = {"fabricName": "test", "seq": seq_p,
                                  "description": ""}
                    flow_route["inDevice"] = {"deviceAlias": nodes[route_u_i_v[0][0]]["deviceName"], "portName": idx_to_if(
                        nodes[route_u_i_v[0][0]]["deviceName"], self.server[route_u_i_v[0][0]][route_u_i_v[0][1]][1])}
                    flow_route["passDeviceList"] = []
                    for i in range(1, len(route_u_i_v)-1):
                        p_route = {}
                        p_route["deviceAlias"] = nodes[route_u_i_v[i]
                                                       [2]]["deviceName"]
                        p_route["portName"] = links[nodes[route_u_i_v[i][0]]["deviceName"]
                                                    ][nodes[route_u_i_v[i][2]]["deviceName"]][route_u_i_v[i][1]][1]
                        flow_route["passDeviceList"].append(p_route)
                    flow_route["outDevice"] = {
                        "deviceAlias": nodes[route_u_i_v[len(route_u_i_v)-1][2]]["deviceName"]}
                    flow_route["outDevice"]["portName"] = links[nodes[route_u_i_v[len(
                        route_u_i_v)-1][0]]["deviceName"]][nodes[route_u_i_v[len(route_u_i_v)-1][2]]["deviceName"]][route_u_i_v[len(route_u_i_v)-1][1]][1]
                    flow_route["fabricFlowPathRuleEntityList"] = []
                    for i in range(len(self.server[v])):
                        sd = {}
                        sd["seq"] = seq_f
                        sd["action"] = "permit"
                        sd["srcIp"] = self.server[route_u_i_v[0][0]][u_i][0]
                        sd["dstIp"] = self.server[route_u_i_v[0][2]][i][0]
                        flow_route["fabricFlowPathRuleEntityList"].append(sd)
                        seq_f += 1
                        flow_route["seq"] = seq_p
                    seq_p += 1
                    print(flow_route)
                    save_route(ip, flow_route)
# seq+=1


non = nonblocking()
non.get_s_to_d_routing()
