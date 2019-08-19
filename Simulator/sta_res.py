#-*-coding:utf-8-*-
import os



E_0 = 0  # 细粒度写均衡
E_1 = 1  # 粗粒度写均衡    先执行E_h_init_info_fine.py 初始化   并且需要在src目录下有 info_文件
E_2 = 2  # 传统策略写均衡
E_3 = 3  # 不加写均衡

# # # # # # # # # #
P = 1
L = 2


E = 0
order = P
fineness = 0  # 重构粒度
file_num = 100  #配置文件个数





if (E == E_0):
    E_path = "e_0/"
elif (E == E_1):
    E_path = "e_1/"
elif (E == E_2):
    E_path = "e_2/"
elif (E == E_3):
    E_path = "e_3/"



e_root_path = "/home/zhlab/BRAM/"+E_path
dir_name = os.listdir(e_root_path)
print(dir_name)

for benchmark in dir_name:
    root_path = e_root_path+benchmark
    sta_path ="/mnt/zhlab/BRAM/"+E_path[0:len(E_path)-1]+"_"+benchmark+".txt"
    res_path = root_path + "/res/"
    src_path = root_path + "/src/"
    efficiency_path = src_path + "sythesis_efficiency.txt"
    performance_path = src_path + "performance.txt"
    cur_limt_path = src_path + "cur_limit.txt"
    # print(sta_path)
    if(os.path.exists(cur_limt_path) == 1):





        times_all = 0               # 总周期
        sythesis_all = 0            # 综合次数
        first_time = 0              # 单次最大周期


        l_delay_all = 0
        t_delay_all = 0
        c_path_all = 0
        l_delay_avg = 0
        t_delay_avg = 0
        c_path_avg = 0
        times_avg = 0


        c_path = []                 # 关键路径
        c_path.clear()
        l_delay = []                # 逻辑延迟
        l_delay.clear()
        t_delay = []                # 总延迟
        t_delay.clear()
        time_vec = []                   # 单次执行周期
        time_vec.clear()

        order_vec = []
        order_vec.clear()
        order_dict = {}
        order_dict.clear()


        efficiency_file = open (efficiency_path )
        for line in efficiency_file:
            line_ = line.split()
            if (len(line_) == 2):
                time_vec.append(line_[1])
        efficiency_file.close()

        performance_file = open (performance_path)

        for line in performance_file:
            line_ = line.split()
            if (len(line_) == 4):
                l_delay.append(line_[1])
                t_delay.append(line_[2])
                c_path.append(line_[3])
        performance_file.close()



        if (len(time_vec) == len(c_path)):
            print("OK")


        first_time = time_vec[0]
        select_tmie = int(fineness * int(first_time))

        del_vec = []
        del_vec.clear()
        for i in range(0,len(time_vec)):
            tmp_num = int(time_vec[i])
            if( tmp_num < select_tmie ):
                del_vec.append(i)

        del_vec.reverse()
        for i in range(0,len(del_vec)):
            del time_vec[del_vec[i]]
            del l_delay[del_vec[i]]
            del t_delay[del_vec[i]]
            del c_path[del_vec[i]]



        if(order == P):
            for i in range(0, len(time_vec)):
                order_dict[i] = float(c_path[i])
            order_dict_ = sorted(order_dict.items(), key=lambda x: x[1])
        elif(order == L):
            for i in range(0, len(time_vec)):
                order_dict[i] = int(time_vec[i])
            order_dict_ = sorted(order_dict.items(), key=lambda x: x[1], reverse=True) #

        tmp_i = 0
        for order_dict_key in order_dict_:
            if (tmp_i < file_num):
                order_vec.append(order_dict_key[0])
            tmp_i += 1

        time_vec_ = time_vec.copy()
        l_delay_ = l_delay.copy()
        t_delay_ = t_delay.copy()
        c_path_ = c_path.copy()
        time_vec.clear()
        l_delay.clear()
        t_delay.clear()
        c_path.clear()
        for i in range(0,len(order_vec)):
            time_vec.append(time_vec_[order_vec[i]])
            l_delay.append(l_delay_[order_vec[i]])
            t_delay.append(t_delay_[order_vec[i]])
            c_path.append(c_path_[order_vec[i]])



        for i in range(0,len(time_vec)):
            times_all += int(time_vec[i])
            t_delay_all += float(t_delay[i])
            l_delay_all += float(l_delay[i])
            c_path_all += float(c_path[i])


        times_avg = (times_all/len(time_vec))
        c_path_avg = (c_path_all/len(time_vec))

        # print(c_path_avg_tmp)
        res_file = open(sta_path,'w')
        line_0 = "1\t" + str(time_vec[0]) + "\t"+str(c_path[0]) +"\t"+str(len(time_vec)) +"\t"+str(times_all)+"\t"+str(int(times_avg))+"\t"+str(c_path_avg)+"\n"
        res_file.write(line_0)
        for i in range(1,len(time_vec)):
            line = str(i+1)+"\t"+ str(time_vec[i]) + "\t"+str(c_path[i]) +"\n"
            res_file.write(line)
        res_file.close()
