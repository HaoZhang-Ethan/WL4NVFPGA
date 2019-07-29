#-*-coding:utf-8-*-

import os

E_0 = 0     #细粒度写均衡
E_1 = 1     #对比实验VPR原始
E_2 = 2     #粗粒度写均衡

E = 0
if(E == E_0):
    E_path = "s_run/"
elif(E == E_1):
    E_path = "s_run_e_1/"
elif(E == E_2):
    E_path = "s_run_e_2/"



benchmark = "LU8PEEng"
E_root_path = "/home/zhlab/BRAM/"
benchmark_src_path = E_root_path+E_path+benchmark+"/src/"
benchmark_res_path = E_root_path+E_path+benchmark+"/res/"
benchmark_pre_info_src_path = E_root_path+E_path+benchmark+"/src/pre_info_src/"
cur_limit_path = E_root_path+E_path+benchmark+"/src/cur_limit.txt"
record_2_path =  E_root_path+E_path+benchmark+"/src/sythesis_efficiency.txt"
write_num_path_pre = E_root_path+E_path+benchmark+"/res/address/"
benchmark_BRAM_path = E_root_path+E_path+benchmark+"/res/BRAM/"
place_res_path = E_root_path+E_path+benchmark+"/res/place_res/"
xml = E_root_path+E_path+benchmark+"/src/pre_info_src/k6_frac_N10_mem32K_40nm_1.xml"
blif = E_root_path+E_path+benchmark+"/src/pre_info_src/"+benchmark+".blif"
place_file = E_root_path+E_path+benchmark+"/src/pre_info_src/"+benchmark+".place"
info_file_path = E_root_path+E_path+benchmark+"/src/LU8PEEng.info_"
addr_hit_num_path = E_root_path+E_path+benchmark+"/res/address/"
forbid_file_path = benchmark_res_path+"forbid_pos"
e_3_bram_file_path = benchmark_BRAM_path+"e_2_bram"
phy_file_path = benchmark_pre_info_src_path + benchmark  + "_phy.txt"
#ace
add_pre_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/ace_pool/"
#tool_path
update_path = "/home/zhlab/FPGA_NVBRAM/SRC/BRAM_writer/update"
trigger_path = "/home/zhlab/FPGA_NVBRAM/SRC/trigger/trigger"



if(E == E_0):
    vpr = "/home/zhlab/FPGA_NVBRAM/SRC/Sythesis/vtr_place_7_5/vpr/vpr"
elif(E == E_1):
    vpr = "/home/zhlab/FPGA_NVBRAM/SRC/Sythesis/vtr_e_1_7_15/vpr/vpr"
elif(E == E_2):
    vpr = "/home/zhlab/FPGA_NVBRAM/SRC/Sythesis/vtr_h/vpr/vpr"






ratio = 0.2
nodisply = "--nodisp"
place = "-place"
time = 0
begin_limit = 0



CONF = 64
COL = 2
ROW = 256
MEM_COL = CONF
MEM_ROW = COL * ROW
MAX_WRITE_NUM = 100000 #1000000      #1000000            #BRAM的最大写次数  //100000000
TEST_NUM = 2
MAX_ADD_PIN = 15
CLK = 5000
MAX_INIT = 200

BIT_DICT = {}
for i in range(0,MAX_ADD_PIN+1):
    BIT_DICT[pow(2,i)]= i


# 获取每个BRAM位置坐标

class INIT_POS:
    def __init__(self):
        self.name = ""
        self.x = -1
        self.y = -1


class INIT_LIST_POS:
    def __init__(self):
        self.init_num = 0
        self.init_lists = []
        for i in range(MAX_INIT):
            self.init_lists.append(INIT_POS())

def GET_BRAM_POS(benchmark_src_path,benchmark_pre_info_src_path,benchmark):
    NO = -1
    INIT_BEGIN = 1
    INIT_NAME = 2


    init_info_file_path = benchmark_src_path+benchmark+".info"
    placement_file_path = benchmark_pre_info_src_path+benchmark+".place"
    pos_info_path = benchmark_src_path+benchmark+"_pos.info"

    Flag = NO
    flag = NO
    inits = INIT_LIST_POS()
    # 对实例命名 统计实例个数
    init_info_file = open(init_info_file_path)
    for line_ in init_info_file:
        if (flag == INIT_NAME):
            inits.init_lists[inits.init_num].name =line_.replace("\n","")
            inits.init_num += 1
            flag = NO
        if (Flag == INIT_BEGIN):
            if (line_.find("INIT_NAME")!=-1):
                flag = INIT_NAME
        if (line_.find("INIT_BEGIN")!=-1):
            Flag = INIT_BEGIN
    init_info_file.close()
    # 给实例设置坐标值
    placement_file = open(placement_file_path)
    for line_ in placement_file:
        tmp_info = line_.split()
        if (len(tmp_info) == 5 and tmp_info[3] == '0'):
            # print(tmp_info[0])
            for i in range (inits.init_num):
                if (tmp_info[0] == inits.init_lists[i].name):
                    inits.init_lists[i].x = tmp_info[1]
                    inits.init_lists[i].y = tmp_info[2]
    placement_file.close()
    pos_info = open(pos_info_path, "w")
    pos_info.write("init_num\n")
    pos_info.write(str(inits.init_num)+"\n")
    for i in range(inits.init_num):
        pos_info.write(inits.init_lists[i].name+" "+str(inits.init_lists[i].x)+" "+str(inits.init_lists[i].y)+"\n")
    pos_info.close()

def BUILD_INIT_POS_DICT(pos_dict,benchmark_src_path,benchmark):
    pos_file_path = benchmark_src_path+benchmark+"_pos.info"
    pos_file = open(pos_file_path)
    for line in pos_file:
        line_ = line.split()
        if (len(line_)==3):
            # print(line_)
            mem_name = line_[1]+"_"+line_[2]+"mem"
            pos_dict[line_[0]] = mem_name




class INIT:
    def __init__(self):
        self.init_name = ""
        self.write_enable = ""
        self.mode = ""
        self.real_add_num = 0
        self.add_num = 0
        self.A = ["0" for i in range(MAX_ADD_PIN)]  # pin口引脚
        self.S_Array = [[0] * (TEST_NUM* CLK) for i in range((MAX_ADD_PIN + 1))]
        self.hit_add_dict = {}
        self.write_num = 0
    def my_init(self,dict_num):
        for i in range (dict_num):
            self.hit_add_dict[i] = 0

class INIT_LIST:
    def __init__(self):
        self.init_num = 0
        self.init_list = []
        for i in range (0,MAX_INIT):#构造实例列表 每个实例可以代表一个BRAM块 构造500块
            self.init_list.append(INIT())
# def CREAT_INIT(inits):
#     info_file_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/LU8PEEng.info_"
#     NO = -1
#     Flag = NO
#     flag = NO
#     INIT_BEGIN = 1
#     INIT_NAME = 2
#     INIT_MODE = 3
#     WE = 4
#     ADDRESS_BEGIN = 5
#     file = open(info_file_path)
#     init_count = 0
#     for line in file:
#         if (line.find("INIT_END") != -1):
#             inits.init_num = init_count
#             break
#         if (flag == ADDRESS_BEGIN):
#             if (line.find("ADDRESS_END") != -1):
#                 inits.init_list[init_count].real_add_num = real_pin_used
#                 init_count += 1
#                 flag = NO
#                 continue
#             else:
#                 tmp_begin = line.find("]=")
#                 key_info = line.replace("\n", "")[tmp_begin + 2:len(line)]
#                 if key_info == "":
#                     real_pin_used -= 1
#                 inits.init_list[init_count].A[pin_count] = key_info
#                 pin_count += 1
#                 continue
#         elif (flag == WE):
#             inits.init_list[init_count].write_enable = line.replace("\n", "").replace(" ","")
#             flag = NO
#             continue
#         elif (flag == INIT_MODE):
#             inits.init_list[init_count].mode = line.replace("\n", "")
#             tmp_mode_end = inits.init_list[init_count].mode.find("x")
#             add_range = inits.init_list[init_count].mode[4:tmp_mode_end]
#             inits.init_list[init_count].my_init(int(add_range))
#             inits.init_list[init_count].add_num = BIT_DICT[int(add_range)]
#             flag = NO
#             continue
#         elif (flag == INIT_NAME):
#             inits.init_list[init_count].init_name = line.replace("\n", "")
#             flag = NO
#             continue
#         if (Flag == INIT_BEGIN):
#             if (line.find("INIT_NAME") != -1):
#                 flag = INIT_NAME
#             elif (line.find("INIT_MODE") != -1):
#                 flag = INIT_MODE
#             elif (line.find("WE") != -1):
#                 flag = WE
#             elif (line.find("ADDRESS_BEGIN") != -1):
#                 flag = ADDRESS_BEGIN
#                 pin_count = 0
#                 real_pin_used = MAX_ADD_PIN
#         if (line.find("INIT_BEGIN") != -1):
#             Flag = INIT_BEGIN
#     print("get address pin info")
#

def GET_ADDR_WRITE_NUM(inits,info_file_path,E):
    NO = -1
    Flag = NO
    flag = NO
    INIT_BEGIN = 1
    INIT_NAME = 2
    INIT_MODE = 3
    WE = 4
    ADDRESS_BEGIN = 5
    file = open(info_file_path)
    init_count = 0
    for line in file:
        if (line.find("INIT_END") != -1):
            inits.init_num = init_count
            break
        if (flag == ADDRESS_BEGIN):
            if (line.find("ADDRESS_END") != -1):
                inits.init_list[init_count].real_add_num = real_pin_used
                init_count += 1
                flag = NO
                continue
            else:
                tmp_begin = line.find("]=")
                key_info = line.replace("\n", "")[tmp_begin + 2:len(line)]
                if key_info == "":
                    real_pin_used -= 1
                inits.init_list[init_count].A[pin_count] = key_info
                pin_count += 1
                continue
        elif (flag == WE):
            inits.init_list[init_count].write_enable = line.replace("\n", "").replace(" ","")
            flag = NO
            continue
        elif (flag == INIT_MODE):
            inits.init_list[init_count].mode = line.replace("\n", "")
            tmp_mode_end = inits.init_list[init_count].mode.find("x")
            add_range = inits.init_list[init_count].mode[4:tmp_mode_end]
            inits.init_list[init_count].my_init(int(add_range))
            inits.init_list[init_count].add_num = BIT_DICT[int(add_range)]
            flag = NO
            continue
        elif (flag == INIT_NAME):
            inits.init_list[init_count].init_name = line.replace("\n", "")
            flag = NO
            continue
        if (Flag == INIT_BEGIN):
            if (line.find("INIT_NAME") != -1):
                flag = INIT_NAME
            elif (line.find("INIT_MODE") != -1):
                flag = INIT_MODE
            elif (line.find("WE") != -1):
                flag = WE
            elif (line.find("ADDRESS_BEGIN") != -1):
                flag = ADDRESS_BEGIN
                pin_count = 0
                real_pin_used = MAX_ADD_PIN
        if (line.find("INIT_BEGIN") != -1):
            Flag = INIT_BEGIN
    print("get address pin info")
    if(E == E_0):
        for init_num in range(inits.init_num):
            tmp_res = [""] * CLK
            for i in range(0, 2):
                s_file_path = add_pre_path + str(i) + ".ace"
                s_file = open(s_file_path)
                for line in s_file:
                    line_ = line.replace("\n", "").split()
                    if (line_[0] == inits.init_list[init_num].write_enable):
                        tmp_res.clear()
                        tmp_res = line_[1:len(line_)]
                        tmp_res = list(map(int, tmp_res))
                        if (i == 0):
                            inits.init_list[init_num].S_Array[MAX_ADD_PIN][:CLK] = tmp_res
                            continue
                        else:
                            inits.init_list[init_num].S_Array[MAX_ADD_PIN][CLK:] = tmp_res
                            continue
                    for init_pin_num in range(0, inits.init_list[init_num].add_num):
                        if (line_[0] == inits.init_list[init_num].A[init_pin_num]):
                            tmp_res.clear()
                            tmp_res = line_[1:len(line_)]
                            tmp_res = list(map(int, tmp_res))
                            if (i == 0):
                                inits.init_list[init_num].S_Array[init_pin_num][:CLK] = tmp_res
                                continue
                            else:
                                inits.init_list[init_num].S_Array[init_pin_num][CLK:] = tmp_res
                                continue
                s_file.close()
        print("get pin status")
    elif (E == E_1):
        for init_num in range(inits.init_num):
            tmp_res = [""] * CLK
            for i in range(0, 2):
                s_file_path = add_pre_path + str(i) + ".ace"
                s_file = open(s_file_path)
                for line in s_file:
                    line_ = line.replace("\n", "").split()
                    if (line_[0] == inits.init_list[init_num].write_enable):
                        tmp_res.clear()
                        tmp_res = line_[1:len(line_)]
                        tmp_res = list(map(int, tmp_res))
                        if (i == 0):
                            inits.init_list[init_num].S_Array[MAX_ADD_PIN][:CLK] = tmp_res
                            continue
                        else:
                            inits.init_list[init_num].S_Array[MAX_ADD_PIN][CLK:] = tmp_res
                            continue
                s_file.close()
        for init_num in range(inits.init_num):
            # print(inits.init_list[init_num].S_Array[MAX_ADD_PIN].count(1))
            inits.init_list[init_num].write_num = inits.init_list[init_num].S_Array[MAX_ADD_PIN].count(1)
            # print(inits.init_list[init_num].write_num)


    if(E == E_0):
        for init_num in range(inits.init_num):
            for pin_num in range (inits.init_list[init_num].real_add_num):
                if (inits.init_list[init_num].A[pin_num] == "gnd"):
                    inits.init_list[init_num].S_Array[pin_num] = [0]*len(inits.init_list[init_num].S_Array[15])
                    # for tmp_i in range (len(inits.init_list[init_num].S_Array[pin_num])):
                    #     inits.init_list[init_num].S_Array[pin_num][tmp_i] = 0
                    # print("")
                elif (inits.init_list[init_num].A[pin_num] == "vcc"):
                    inits.init_list[init_num].S_Array[pin_num] = [1] * len(inits.init_list[init_num].S_Array[15])
                    # for tmp_i in range(len(inits.init_list[init_num].S_Array[pin_num])):
                    #     inits.init_list[init_num].S_Array[pin_num][tmp_i] = 1
                    # print("")
        print("get pin status")
    if(E == E_0):
        for init_num in range(inits.init_num):
            for clk_ in range(CLK * TEST_NUM):
                if inits.init_list[init_num].S_Array[MAX_ADD_PIN][clk_] == 1:
                    add_acc_ = ""
                    for real_add_num in range(0, inits.init_list[init_num].add_num):
                        add_acc_ += str(inits.init_list[init_num].S_Array[real_add_num][clk_])
                    hit_add = int(add_acc_, 2)
                    # print(hit_add)
                    inits.init_list[init_num].hit_add_dict[hit_add] += 1
        print("get hit_add_dict")
    if(E == E_0):
        for init_num in range(0, init_count):
            all_no_flag = 0
            count_add_hit_num_file_path = addr_hit_num_path + str(init_num) +".hit"
            count_add_hit_num_file = open(count_add_hit_num_file_path, 'w')
            count_add_hit_num_file.write("mem_name\n")
            count_add_hit_num_file.write(inits.init_list[init_num].init_name+"\n")
            count_add_hit_num_file.write("mem_mode\n")
            count_add_hit_num_file.write(inits.init_list[init_num].mode + "\n")
            count_add_hit_num_file.write("add_begin\n")
            tmp_mode_end = inits.init_list[init_num].mode.find("x")
            add_range = inits.init_list[init_num].mode[4:tmp_mode_end]
            add_range = int(add_range)
            for i in range(add_range):
                if (inits.init_list[init_num].hit_add_dict[i] != 0):
                    count_add_hit_num_file.write(str(i) + " " + str(inits.init_list[init_num].hit_add_dict[i]) + "\n")
                    all_no_flag = 1
            if (all_no_flag == 0):
                count_add_hit_num_file.write("0 0\n")
            count_add_hit_num_file.close()
    if(E == E_1):
        for init_num in range(0, init_count):
            all_no_flag = 0
            count_add_hit_num_file_path = addr_hit_num_path + str(init_num) +".hit"
            count_add_hit_num_file = open(count_add_hit_num_file_path, 'w')
            count_add_hit_num_file.write("mem_name\n")
            count_add_hit_num_file.write(inits.init_list[init_num].init_name+"\n")
            count_add_hit_num_file.write("mem_mode\n")
            count_add_hit_num_file.write(inits.init_list[init_num].mode + "\n")
            count_add_hit_num_file.write("add_begin\n")
            count_add_hit_num_file.write(str(inits.init_list[init_num].write_num)+"\n")
            count_add_hit_num_file.close()



#检测是否写到上限
def trigger(benchmark_BRAM_path,trigger_path,pos_dict,up_limit):
    for mem in pos_dict.values():
        mem_file_path = benchmark_BRAM_path + mem
        command_trigger = trigger_path +" " + mem_file_path +" "+ str(int(up_limit))
        # print(command_trigger)
        res = str(os.system(command_trigger))
        # print(res)
        # print(len(res))
        # print("")
        if (len(res) > 2):
            return 0
    return 1

def get_init_info(cur_limit_path):
    cur_limit_file = open(cur_limit_path)
    for line in cur_limit_file:
        if(line.find("cur_limit")!=-1):
            line_ = line.split()
            cur_limit = int(line_[2])
        elif(line.find("times")!=-1):
            line_ = line.split()
            times = int(line_[2])
        elif(line.find("sythesis_time")!=-1):
            line_ = line.split()
            sythesis_time = int(line_[2])
        elif(line.find("E_limit")!= -1):
            line_ = line.split()
            E_limit = int(line_[2])
    cur_limit_file.close()
    return cur_limit,times,sythesis_time,E_limit


def record_1(path,cur_limit,times,sythesis_time,E_limit):
    cur_limit_file = open(path, 'w')
    cur_limit_file.write("cur_limit = " + str(int(cur_limit)));
    cur_limit_file.write("\ntimes = " + str(times));
    cur_limit_file.write("\nsythesis_time = " + str(sythesis_time));
    cur_limit_file.write("\nE_limit = "+str(E_limit));
    cur_limit_file.close()


#记录综合效率
def record_2(path,num,cur_limit):
    file = open(path, 'a')
    file.write(str(cur_limit)+"\t"+str(num)+"\n")
    file.close()



def e_3_get_build_dict(file_path,opt):
    INT = 1
    FLOAT = 2
    bram_pos_dict = {}
    bram_file = open(file_path)
    for line in bram_file:
        if(len(line)>1):
            line_ = line.split()
        tmp_pos = line_[0]+"_"+line_[1]
        if (opt == INT):
            bram_pos_dict[tmp_pos] = int(line_[2])
        if (opt == FLOAT):
            bram_pos_dict[tmp_pos] = float(line_[2])
    bram_file.close()
    return bram_pos_dict
def update_e_3_phy(file_path,num_dict):
    file = open(file_path,'w')
    for key in num_dict.keys():
        tmp_write_num = str(num_dict[key])
        tmp_str = key.replace("_"," ")+" "+tmp_write_num+"\n"
        file.write(tmp_str)
    file.close()
def update_e_3_phy_dict(cur_limit,bram_pos_dict):
    phy_ratio_dict = {}
    for key in bram_pos_dict.keys():
        tmp_write_num = int(bram_pos_dict[key])
        if(tmp_write_num == 0):
            phy_ratio_dict[key] = 0
        else:
            tmp = tmp_write_num/cur_limit
            if(tmp > 1 ):
                phy_ratio_dict[key] = 1
            else:
                phy_ratio_dict[key] = tmp
    return phy_ratio_dict


# 判断综合效率
def Effic_fun(Effic,Effic_num,E_limit):
    if (Effic_num < 10):
        return 0     # 采样不多不足以判断，返回0
    else:
        sum = 0
        for It_Effic in range(0,10):
            sum += Effic[It_Effic]
        if ((sum/10) < E_limit):
            return 1 # 此时利用率不高，提升up_limit

if(E == E_0):


    if(os.path.exists(cur_limit_path)==1):
        print("continue\n")
        cur_limit,times,sythesis_time,E_limit = get_init_info(cur_limit_path)
        cur_limit = cur_limit - ratio * MAX_WRITE_NUM

    else:
        print("new test\n")
        cur_limit = begin_limit + ratio * MAX_WRITE_NUM
        times = 0
        sythesis_time = 0
        file = open(record_2_path,'w')
        file.write("")
        file.close()
        E_limit = 0
    Effic_num = 0
    Effic = [0]*10
    E_flag = 0
    while(cur_limit < MAX_WRITE_NUM +1):
            record_1(cur_limit_path, cur_limit, times, sythesis_time,E_limit)


            cmd = vpr + " " + xml + " " + blif +" " + nodisply + " " + place + " " + "***" + str(int(cur_limit))+">"+benchmark_res_path+"vpr.out"
            # cmd = vpr + " " + xml + " " + blif +" " + nodisply + " " + place +">>"+benchmark_res_path+"vpr.out"
            os.system(cmd)
            flag = os.path.exists(place_file)
            if (flag == 1):

                old_tmies = times
                sythesis_time += 1
                Effic_num += 1

                cmd_transfrom = "python /home/zhlab/FPGA_NVBRAM/SRC/Simulator/S_transform.py"
                os.system(cmd_transfrom)
                mv_place_pin = "mv "+benchmark_pre_info_src_path+benchmark+".place_pin"+" "+ benchmark_res_path+"place_pin/"
                mv_place_pin = mv_place_pin + str(sythesis_time) + ".place_pin"
                os.system(mv_place_pin)
                GET_BRAM_POS(benchmark_src_path, benchmark_pre_info_src_path,benchmark)
                pos_dict = {}
                BUILD_INIT_POS_DICT(pos_dict, benchmark_src_path, benchmark)
                inits = INIT_LIST()
                GET_ADDR_WRITE_NUM(inits,E_root_path+E_path+benchmark+"/src/"+benchmark+".info_",E)
                update_Flag = 1
                while (update_Flag):
                    print("times  =  " + str(times))
                    times += 1
                    for init_ in range(inits.init_num):
                        write_num_path = write_num_path_pre+str(init_)+".hit"
                        write_num = open(write_num_path)
                        name_flag = 0
                        init_name = ""
                        for line_ in write_num:
                            if(name_flag == 1):
                                init_name = line_.replace("\n","")
                                break
                            if(line_.find("mem_name")!=-1):
                                name_flag = 1
                        write_num.close()
                        BRAM_file_path = pos_dict[init_name]
                        print("Hit = "+str(init_)+  "      BRAM_POS = " + BRAM_file_path)
                        BRAM_file_path = benchmark_BRAM_path + BRAM_file_path
                        command = update_path +" " + write_num_path + " " +BRAM_file_path + " "+ str(int(cur_limit))
                        # print(command)
                        res = str(os.system(command))
                        print(res)
                        if(int(res) != 0):
                            print("Write_over")
                            update_Flag = 0

                new_tmies = times
                if(sythesis_time == 1 or E_flag == 1):
                    E_limit = (new_tmies-old_tmies)
                    E_flag = 0

                Effic[sythesis_time%10] = new_tmies-old_tmies

                record_2(record_2_path, new_tmies-old_tmies,cur_limit)

                mv_place_cmd = "mv " + place_file + " " +place_res_path + str(sythesis_time)+".place"
                os.system(mv_place_cmd)

                record_1(cur_limit_path, cur_limit, times, sythesis_time,E_limit)

            if(flag != 1 or Effic_fun(Effic,Effic_num,int(E_limit/4))):#
                cur_limit = cur_limit + ratio * MAX_WRITE_NUM
                E_flag = 1

elif(E == E_1):
    sythesis_time = 0
    times = 0
    cur_limit = MAX_WRITE_NUM
    forbid_list = []
    while (1):
        record_1(cur_limit_path, cur_limit, times, sythesis_time)
        # cmd = vpr + " " + xml + " " + blif +" " + nodisply + " " + place + " " + "***" + str(int(cur_limit))+">>"+benchmark_res_path+"vpr.out"
        cmd = vpr + " " + xml + " " + blif + " " + nodisply + " " + place + ">" + benchmark_res_path + "vpr.out"
        os.system(cmd)
        flag = os.path.exists(place_file)
        if (flag == 1):
            GET_BRAM_POS(benchmark_src_path, benchmark_pre_info_src_path, benchmark)
            pos_dict = {}
            BUILD_INIT_POS_DICT(pos_dict, benchmark_src_path, benchmark)
            inits = INIT_LIST()
            GET_ADDR_WRITE_NUM(inits,E_root_path+E_path+benchmark+"/src/"+benchmark+".info",E)
            update_Flag = 1
            while (update_Flag):
                print("times  =  " + str(times))
                times += 1
                for init_ in range(inits.init_num):
                    write_num_path = write_num_path_pre + str(init_) + ".hit"
                    write_num = open(write_num_path)
                    name_flag = 0
                    init_name = ""
                    for line_ in write_num:
                        if (name_flag == 1):
                            init_name = line_.replace("\n", "")
                            name_flag = 0
                            continue
                        elif (name_flag == 2):
                            init_write_num = int(line_.replace("\n", ""))
                            break
                        if (line_.find("mem_name") != -1):
                            name_flag = 1
                        elif (line_.find("add_begin") != -1):
                            name_flag = 2
                    write_num.close()
                    BRAM_file_path = pos_dict[init_name]
                    print("Hit = " + str(init_) + "      BRAM_POS = " + BRAM_file_path)
                    BRAM_file_path = benchmark_BRAM_path + BRAM_file_path
                    BRAM_file = open(BRAM_file_path,'r+')
                    lines = BRAM_file.readlines()
                    d = ""
                    tmp_flag = 0
                    for line in lines:
                        if (tmp_flag == 1):
                            c = int(line)+init_write_num
                            if(c > MAX_WRITE_NUM):
                                tmp_str_x = BRAM_file_path[BRAM_file_path.rfind("/")+1:BRAM_file_path.rfind("_")]
                                tmp_str_y = BRAM_file_path[BRAM_file_path.rfind("_")+1:BRAM_file_path.rfind("mem")]
                                tmp_str = tmp_str_x + " " +tmp_str_y +" "+ "*"
                                forbid_list.append(tmp_str)
                                update_Flag = 0
                            line = str(c)
                            tmp_flag = 0
                            c = line
                            d += c
                            break
                        if (line.find("use") != -1):
                            tmp_flag = 1
                        c = line
                        d += c
                    BRAM_file.seek(0)  # 不要让python记住执行到这里，从文件头还始
                    BRAM_file.truncate()#清空文件
                    BRAM_file.write(d)
                    BRAM_file.close()
            write_forbit_file = open(forbid_file_path,'w')
            for forbid_list_ in forbid_list:
                write_forbit_file.write(forbid_list_ + "\n")
            write_forbit_file.close()
            mv_place_cmd = "mv " + place_file + " " + place_res_path + str(sythesis_time) + ".place"
            os.system(mv_place_cmd)
            sythesis_time += 1
        if (flag != 1):
            break
            print("end")
elif(E == E_2):
    if(os.path.exists(cur_limit_path)==1):
        print("continue\n")
        cur_limit,times,sythesis_time,E_limit = get_init_info(cur_limit_path)
        cur_limit = cur_limit - ratio * MAX_WRITE_NUM
    else:
        print("new test\n")
        cur_limit = begin_limit + ratio * MAX_WRITE_NUM
        times = 0
        sythesis_time = 0
        file = open(record_2_path,'w')
        file.write("")
        file.close()
        Effic_num = 0

    Effic_num = 0
    Effic = [0]*10
    E_flag = 0
    bram_pos_dict = e_3_get_build_dict(e_3_bram_file_path,1)
    # write_num_dict = e_3_get_build_dict(phy_file_path,2)
    while (cur_limit < MAX_WRITE_NUM +1):
        record_1(cur_limit_path, cur_limit, times, sythesis_time,E_limit)
        phy_ratio_dict = update_e_3_phy_dict( cur_limit, bram_pos_dict)
        update_e_3_phy(phy_file_path, phy_ratio_dict)
        cmd = vpr + " " + xml + " " + blif + " " + nodisply + " " + place + ">" + benchmark_res_path + "vpr.out"
        os.system(cmd)
        flag = os.path.exists(place_file)
        if (flag == 1):

            old_tmies = times
            sythesis_time += 1
            GET_BRAM_POS(benchmark_src_path, benchmark_pre_info_src_path, benchmark)
            pos_dict = {}
            BUILD_INIT_POS_DICT(pos_dict, benchmark_src_path, benchmark)
            inits = INIT_LIST()
            GET_ADDR_WRITE_NUM(inits,E_root_path+E_path+benchmark+"/src/"+benchmark+".info",E_1)
            update_Flag = 1
            while (update_Flag):
                print("times  =  " + str(times))
                times += 1
                for init_ in range(inits.init_num):
                    write_num_path = write_num_path_pre + str(init_) + ".hit"
                    write_num = open(write_num_path)
                    name_flag = 0
                    init_name = ""
                    for line_ in write_num:
                        if (name_flag == 1):
                            init_name = line_.replace("\n", "")
                            name_flag = 0
                            continue
                        elif (name_flag == 2):
                            init_write_num = int(line_.replace("\n", ""))
                            break
                        if (line_.find("mem_name") != -1):
                            name_flag = 1
                        elif (line_.find("add_begin") != -1):
                            name_flag = 2
                    write_num.close()
                    BRAM_file_path = pos_dict[init_name]
                    BRAM_file_path = BRAM_file_path.replace("mem", "")
                    bram_pos_dict[BRAM_file_path] = bram_pos_dict[BRAM_file_path] + init_write_num
                    # tmp_ratio = bram_pos_dict[BRAM_file_path]/cur_limit
                    # if(tmp_ratio > 1):
                    #     tmp_ratio = 1
                    # write_num_dict[BRAM_file_path] = tmp_ratio
                    if(bram_pos_dict[BRAM_file_path] > cur_limit):
                        update_Flag = 0
                update_e_3_phy(e_3_bram_file_path, bram_pos_dict)


            # update_e_3_phy(phy_file_path, write_num_dict)
            mv_place_cmd = "mv " + place_file + " " + place_res_path + str(sythesis_time) + ".place"
            os.system(mv_place_cmd)
            new_tmies = times
            if (sythesis_time == 1 or E_flag == 1):
                E_limit = (new_tmies - old_tmies)
                E_flag = 0
            Effic[sythesis_time % 10] = new_tmies - old_tmies
            record_2(record_2_path, new_tmies - old_tmies, cur_limit)
            record_1(cur_limit_path, cur_limit, times, sythesis_time, E_limit)
        if (flag != 1 or Effic_fun(Effic,Effic_num,int(E_limit/4))):
            cur_limit = cur_limit + ratio * MAX_WRITE_NUM
            E_flag = 1