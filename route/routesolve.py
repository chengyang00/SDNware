
import time

from docplex.mp.model import Model

file_switch_name="switch.txt"
file_topo_name="topo.txt"
file_server_name="server.txt"
class lp_mlu_route:
    def __init__(self,n_node):
        self.n_node=n_node
        self.bw_host=[0 for i in range(self.n_node)]
        self.capacity=[[0 for i in range(self.n_node)] for j in range(self.n_node)]
        self.switchs={}
        
        
        
        file_switch=open(file_switch_name,"r")
        s=file_switch.readline()
        i=0
        while s!="":
            self.switchs[s]=i
            i+=1
            s=file_switch.readline()   
            
        self.f_c=open(file_topo_name,'r')
        s=self.f_c.readline()
        while s!="":
            ss=s.split(" ")
            self.capacity[int(float(ss[0]))][int(float(ss[1]))]=float(ss[2])
            s=self.f_c.readline()


        self.f_h=open(file_server_name,"r")
        
        s=self.f_h.readline()
        while s!="":
            sw=s
            n_server=int(self.f_h.readline())
            for i in range(n_server):
                bw=self.f_h.readline()
                #print(bw,bw.split(" "),self.switchs[sw])
                self.bw_host[self.switchs[sw]]+=float(bw.split(" ")[2])
            s=self.f_h.readline()
        
        #print("sadassadasd",self.bw_host)

        #print(c,t,h)

        #for cc in self.c:
        #    print(cc)
        #print("")
        #print(self.h)
        self.routing={}
        self.mlu=0


    def get_routing(self):
        model=Model(name="routing")

        """变量初始化"""
        f={}
        for u in range(self.n_node):
            if self.bw_host[u]==0:
                continue
            f[u]={}
            self.routing[u]={}
            for v in range(self.n_node):
                if u==v:
                    continue
                if self.bw_host[v]==0:
                    continue
                f[u][v]={}
                self.routing[u][v]={}
                for i in range(self.n_node):
                    f[u][v][i]={}
                    self.routing[u][v][i]={}
                    for j in range(self.n_node):
                        if i==j:
                            continue
                        if self.capacity[i][j]==0:
                            continue
                        f[u][v][i][j]=model.continuous_var(lb=0,name=f"f_{u}_{v}_{i}_{j}")
                        self.routing[u][v][i][j]=0
        lam,eta={},{}
        for u in range(self.n_node):
            if self.bw_host[u]==0:
                continue
            lam[u]={}
            eta[u]={}
            for i in range(self.n_node):
                lam[u][i]={}
                eta[u][i]={}
                for j in range(self.n_node):
                    if i==j:
                        continue
                    if self.capacity[i][j]==0:
                        continue
                    lam[u][i][j]=model.continuous_var(lb=0,name=f"lam_{u}_{i}_{j}")
                    eta[u][i][j]=model.continuous_var(lb=0,name=f"eta_{u}_{i}_{j}")


        for u in range(self.n_node):
            if self.bw_host[u]==0:
                continue
            for v in range(self.n_node):
                if u==v:
                    continue
                if self.bw_host[v]==0:
                    continue
                for i in range(self.n_node):
                    router=model.linear_expr()
                    for j in range(self.n_node):
                        if i==j:
                            continue
                        if self.capacity[i][j]==0:
                            continue
                        router+=f[u][v][i][j]
                        router-=f[u][v][j][i]
                        model.add_constraint(lam[u][i][j]+eta[v][i][j]>=f[u][v][i][j])
                    if i==u:
                        router-=1
                    elif i==v:
                        router+=1
                    model.add_constraint(router==0)

        mlu_v=model.continuous_var(lb=0,name="mlu")
    
        for i in range(self.n_node):
            for j in range(self.n_node):
                if i==j:
                    continue
                if self.capacity[i][j]==0:
                    continue
                ml=model.linear_expr()
                for u in range(self.n_node):
                    if self.bw_host[u]==0:
                        continue
                    ml+=self.bw_host[u]*(lam[u][i][j]+eta[u][i][j])
                model.add_constraint(mlu_v>=ml/self.capacity[i][j])

        model.minimize(mlu_v)

        res=model.solve()

        if res:
            mlu=res[mlu_v]
            #print(mlu)
            for u in range(self.n_node):
                if self.bw_host[u]==0:
                    continue
                for v in range(self.n_node):
                    if u==v:
                        continue
                    if self.bw_host[v]==0:
                        continue
                    for i in range(self.n_node):
                        for j in range(self.n_node):
                            if i==j:
                                continue
                            if self.capacity[i][j]==0:
                                continue
                            self.routing[u][v][i][j]=res[f[u][v][i][j]]

        else:
            print("solve fail")
            print(model.get_solve_status())
            print(model.get_solve_details())
        return res[mlu_v]
    def main(self):
        route=lp_mlu_route(4)
        mlu=route.get_routing()


        #tm=time.time()




        f=open("route.txt",'w')


        for u in route.routing:
            for v in route.routing[u]:
                #print(u,'->',v)
                #print(route.routing[u][v])
                for i in route.routing[u][v]:
                    for j in route.routing[u][v][i]:
                        f.write(f"{u} {v} {i} {j} {route.routing[u][v][i][j]}\n")

        #print("time:",time.time()-tm)