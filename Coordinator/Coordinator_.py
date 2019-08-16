#-*-coding:utf-8-*-


import os
import random
import math

# 实验序号
E_0 = 0  # 细粒度写均衡
E_1 = 1  # 粗粒度写均衡    先执行E_h_init_info_fine.py 初始化   并且需要在src目录下有 info_文件
E_2 = 2  # 传统策略写均衡
E_3 = 3  # 不加写均衡

# # # # # # # # # #
E = 1



if (E == E_0):
    E_path = "e_0/"
elif (E == E_1):
    E_path = "e_1/"
elif (E == E_2):
    E_path = "e_2/"
elif (E == E_3):
    E_path = "e_3/"
elif (E == E_4):
    E_path = "e_4/"



# amp_num 为写放大系数

# benchmark = "boundtop"
# amp_num = 10

# benchmark =  "B1" #"LU8PEEng"
# amp_num = 10

# benchmark = "B3" # "LU32PEEng"
# amp_num = 10

# benchmark =  "mcml"
# amp_num = 1000

# benchmark =  "mkDelayWorker32B"
# amp_num = 1000

benchmark = "B2" #"mkPktMerge"
amp_num = 10

# benchmark =  "mkSMAdapter4B"
# amp_num = 1000

# 路径信息
E_root_path = "/home/zhlab/BRAM/"
benchmark_src_path = E_root_path + E_path + benchmark + "/src/"
benchmark_res_path = E_root_path + E_path + benchmark + "/res/"
benchmark_pre_info_src_path = E_root_path + E_path + benchmark + "/src/pre/"
cur_limit_path = E_root_path + E_path + benchmark + "/src/cur_limit.txt"
record_2_path = E_root_path + E_path + benchmark + "/src/sythesis_efficiency.txt"
write_num_path_pre = E_root_path + E_path + benchmark + "/res/address/"
benchmark_BRAM_path = E_root_path + E_path + benchmark + "/res/BRAM/"
place_res_path = E_root_path + E_path + benchmark + "/res/place_res/"
xml = E_root_path + E_path + benchmark + "/src/pre/A1.xml"
blif = E_root_path + E_path + benchmark + "/src/pre/" + benchmark + ".blif"
place_file = E_root_path + E_path + benchmark + "/src/pre/" + benchmark + ".place"
route_file = E_root_path + E_path + benchmark + "/src/pre/" + benchmark + ".route"
info_file_path = E_root_path + E_path + benchmark + "/src/" + benchmark + ".info_"
addr_hit_num_path = E_root_path + E_path + benchmark + "/res/address/"
forbid_file_path = benchmark_res_path + "forbid_pos"
e_1_bram_file_path = benchmark_BRAM_path + "e_1_bram"
phy_file_path = benchmark_pre_info_src_path + benchmark + "_phy.txt"
# ace
add_pre_path = "/home/zhlab/BRAM/"+ E_path  + benchmark + "/src/ace_pool/"
# tool_path
update_path = "/home/zhlab/FPGA_NVBRAM/SRC/BRAM_writer/update"
trigger_path = "/home/zhlab/FPGA_NVBRAM/SRC/trigger/trigger"
performance_path = benchmark_src_path + "performance.txt"
res_path = benchmark_res_path + "vpr.out"

if (E == E_0):
    vpr = "/home/zhlab/FPGA_NVBRAM/SRC/Sythesis/vpr/vpr0"
elif (E == E_1):
    vpr = "/home/zhlab/FPGA_NVBRAM/SRC/Sythesis/vpr/vpr1"
elif (E == E_2):
    vpr = "/home/zhlab/FPGA_NVBRAM/SRC/Sythesis/vpr/vpr2"
elif (E == E_3):
    vpr = "/home/zhlab/FPGA_NVBRAM/SRC/Sythesis/vpr/vpr3"
elif (E == E_4):
    vpr = "/home/zhlab/FPGA_NVBRAM/SRC/Sythesis/vpr/vpr4"



ratio = [1]       # 每个阶段上限制
ratio_num = len(ratio)           # 阶段
min_ratio = [50]


nodisply = "--nodisp"
place = "--place"
route = "--route"
route_opt = "--route_chan_width 250"
seed = "--seed"


CONF = 64
COL = 2
ROW = 256
MEM_COL = CONF
MEM_ROW = COL * ROW
MAX_WRITE_NUM = 1000000  # 1000000000000     #BRAM的最大写次数  //100000000
TEST_NUM = 2
MAX_ADD_PIN = 15
CLK = 5000
MAX_INIT = 200


time = 0
begin_limit = 0


#
# 确认信息
#
print("E = "+str(E))
print("benchmark = "+benchmark)
print("vpr = "+vpr)
print("MAX_WRITE_NUM = "+str(MAX_WRITE_NUM))
print("ratio = "+str(ratio)+"\t ratio_num = "+ str(ratio_num))
print("min ratio = "+str(min_ratio))

while 1:
    cmd_sure_flag = input("info all right ？ (y/N)")
    if (cmd_sure_flag == 'y'):
        print("info OK")
        break
    elif (cmd_sure_flag == 'N'):
        print("info ERROR")
        exit()
        break
    else :
        print("input error")




#
# 基本函数与结构体
#

# 地址线条数对应的地址范围
BIT_DICT = {}
for i in range(0, MAX_ADD_PIN + 1):
    BIT_DICT[pow(2, i)] = i

# BRAM实例(在建立BRAM坐标位置词典的时候用)
# 单个BRAM
class INIT_POS:
    def __init__(self):
        self.name = ""
        self.x = -1
        self.y = -1
# BRAMs
class INIT_LIST_POS:
    def __init__(self):
        self.init_num = 0
        self.init_lists = []
        for i in range(MAX_INIT):
            self.init_lists.append(INIT_POS())

# 从place文件中读取信息,然后返回BRAM的坐标文件._pos.info
def GET_BRAM_POS(benchmark_src_path, benchmark_pre_info_src_path, benchmark):
    NO = -1
    INIT_BEGIN = 1
    INIT_NAME = 2

    init_info_file_path = benchmark_src_path + benchmark + ".info"
    placement_file_path = benchmark_pre_info_src_path + benchmark + ".place"
    pos_info_path = benchmark_src_path + benchmark + "_pos.info"

    Flag = NO
    flag = NO
    inits = INIT_LIST_POS()
    # 对实例命名 统计实例个数
    init_info_file = open(init_info_file_path)
    for line_ in init_info_file:
        if (flag == INIT_NAME):
            inits.init_lists[inits.init_num].name = line_.replace("\n", "")
            inits.init_num += 1
            flag = NO
        if (Flag == INIT_BEGIN):
            if (line_.find("INIT_NAME") != -1):
                flag = INIT_NAME
        if (line_.find("INIT_BEGIN") != -1):
            Flag = INIT_BEGIN
    init_info_file.close()
    # 给实例设置坐标值
    placement_file = open(placement_file_path)
    for line_ in placement_file:
        tmp_info = line_.split()
        if (len(tmp_info) == 5 and tmp_info[3] == '0'):
            # print(tmp_info[0])
            for i in range(inits.init_num):
                if (tmp_info[0] == inits.init_lists[i].name):
                    inits.init_lists[i].x = tmp_info[1]
                    inits.init_lists[i].y = tmp_info[2]
    placement_file.close()
    pos_info = open(pos_info_path, "w")
    pos_info.write("init_num\n")
    pos_info.write(str(inits.init_num) + "\n")
    for i in range(inits.init_num):
        pos_info.write(inits.init_lists[i].name + " " + str(inits.init_lists[i].x) + " " + str(inits.init_lists[i].y) + "\n")
    pos_info.close()
# 根据_pos.info建立坐标词典
def BUILD_INIT_POS_DICT(pos_dict, benchmark_src_path, benchmark):
    pos_file_path = benchmark_src_path + benchmark + "_pos.info"
    pos_file = open(pos_file_path)
    for line in pos_file:
        line_ = line.split()
        if (len(line_) == 3):
            # print(line_)
            mem_name = line_[1] + "_" + line_[2] + "mem"
            pos_dict[line_[0]] = mem_name

# BRAM
class INIT:
    def __init__(self):
        self.init_name = ""
        self.write_enable = ""
        self.mode = ""
        self.real_add_num = 0
        self.add_num = 0
        self.A = ["0" for i in range(MAX_ADD_PIN)]  # pin口引脚
        self.S_Array = [[0] * (TEST_NUM * CLK) for i in range((MAX_ADD_PIN + 1))]
        self.hit_add_dict = {}
        self.write_num = 0
    def my_init(self, dict_num):
        for i in range(dict_num):
            self.hit_add_dict[i] = 0
# BRAMs
class INIT_LIST:
    def __init__(self):
        self.init_num = 0
        self.init_list = []
        for i in range(0, MAX_INIT):  # 构造实例列表 每个实例可以代表一个BRAM块 构造500块
            self.init_list.append(INIT())


# 地址译码
def GET_ADDR_WRITE_NUM(inits, info_file_path, E, Amp,Sampling = 10):
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
            inits.init_list[init_count].write_enable = line.replace("\n", "").replace(" ", "")
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


    for i in range (0,Sampling):

        if (E == E_0):
            for init_num in range(inits.init_num):
                tmp_res = [""] * CLK

                ace_list = []
                ace_list.clear()
                while (len(ace_list) < 2):
                    rand_num = random.randint(0, 999)
                    if (os.path.exists(add_pre_path + str(rand_num) + ".ace")):
                        ace_list.append(rand_num)

                for i in ace_list:
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

        if (E == E_0):
            for init_num in range(inits.init_num):
                for pin_num in range(inits.init_list[init_num].real_add_num):
                    if (inits.init_list[init_num].A[pin_num] == "gnd"):
                        inits.init_list[init_num].S_Array[pin_num] = [0] * len(inits.init_list[init_num].S_Array[15])
                        # for tmp_i in range (len(inits.init_list[init_num].S_Array[pin_num])):
                        #     inits.init_list[init_num].S_Array[pin_num][tmp_i] = 0
                        # print("")
                    elif (inits.init_list[init_num].A[pin_num] == "vcc"):
                        inits.init_list[init_num].S_Array[pin_num] = [1] * len(inits.init_list[init_num].S_Array[15])
                        # for tmp_i in range(len(inits.init_list[init_num].S_Array[pin_num])):
                        #     inits.init_list[init_num].S_Array[pin_num][tmp_i] = 1
                        # print("")
            print("get pin status")

        if (E == E_0):
            for init_num in range(inits.init_num):
                for clk_ in range(CLK * TEST_NUM):
                    if inits.init_list[init_num].S_Array[MAX_ADD_PIN][clk_] == 1:
                        add_acc_ = ""
                        for real_add_num in range(0, inits.init_list[init_num].add_num):
                            add_acc_ += str(inits.init_list[init_num].S_Array[real_add_num][clk_])
                        hit_add = int(add_acc_, 2)
                        inits.init_list[init_num].hit_add_dict[hit_add] += 1
            print("get hit_add_dict")

    if (E == E_0):
        for init_num in range(0, init_count):
            all_no_flag = 0
            count_add_hit_num_file_path = addr_hit_num_path + str(init_num) + ".hit"
            count_add_hit_num_file = open(count_add_hit_num_file_path, 'w')
            count_add_hit_num_file.write("mem_name\n")
            count_add_hit_num_file.write(inits.init_list[init_num].init_name + "\n")
            count_add_hit_num_file.write("mem_mode\n")
            count_add_hit_num_file.write(inits.init_list[init_num].mode + "\n")
            count_add_hit_num_file.write("add_begin\n")
            tmp_mode_end = inits.init_list[init_num].mode.find("x")
            add_range = inits.init_list[init_num].mode[4:tmp_mode_end]
            add_range = int(add_range)
            for i in range(add_range):
                if (inits.init_list[init_num].hit_add_dict[i] != 0):
                    count_add_hit_num_file.write(
                        str(i) + " " + str(int(math.ceil(inits.init_list[init_num].hit_add_dict[i]/(Sampling/Amp)))) + "\n")
                    all_no_flag = 1
            if (all_no_flag == 0):
                count_add_hit_num_file.write("0 0\n")
            count_add_hit_num_file.close()

# cur_limit文件
def record_1(path, cur_limit, times, sythesis_time, E_limit=0):
    cur_limit_file = open(path, 'w')
    cur_limit_file.write("cur_limit = " + str(int(cur_limit)));
    cur_limit_file.write("\ntimes = " + str(times));
    cur_limit_file.write("\nsythesis_time = " + str(sythesis_time));
    cur_limit_file.write("\nE_limit = " + str(E_limit));
    cur_limit_file.close()
# 记录综合效率
def record_2(path, num, cur_limit):
    file = open(path, 'a')
    file.write(str(cur_limit) + "\t" + str(num) + "\n")
    file.close()
# 记录性能
def record_performance(file_path, line):
    file = open(file_path, 'a')
    file.write(line)
    file.close()
# 获取性能结果
def get_performance(res_path, performance_path, num):
    t_logic_delay = 0
    t_net_delay = 0
    critical_path = 0

    res_file = open(res_path)
    for line in res_file:
        if (line.find("Total logic delay:") != -1):
            line_ = line.split()
            t_logic_delay = line_[3]
            t_net_delay = line_[8]
        elif (line.find("Final critical path:") != -1):
            line_ = line.split()
            critical_path = line_[3]
    res_file.close()

    line_info = str(num) + "\t" + str(t_logic_delay) + "\t" + str(t_net_delay) + "\t" + str(critical_path) + "\n"
    record_performance(performance_path, line_info)
# 判断综合效率
def Effic_fun(Effic, Effic_num, E_limit):
    if (Effic_num < 10):
        return 0  # 采样不多不足以判断，返回0
    else:
        sum = 0
        for It_Effic in range(0, 10):
            sum += Effic[It_Effic]
        if ((sum / 10) < E_limit):
            return 1  # 此时利用率不高，提升up_limit


# 地址格式转换 为了和粗粒度实验匹配
def transfrom_hit(fold_path):
    line_info = []
    for file_name in os.listdir(fold_path):
        hit_file = open(fold_path + file_name)
        i = 0
        Max_num = 0
        line_info.clear()
        for hit_file_line in hit_file:
            if (i < 5):
                line_info.append(hit_file_line.replace("\n", ""))
            else:
                hit_file_line_ = hit_file_line.split()
                num = int(hit_file_line_[1])
                if (num > Max_num):
                    Max_num = num
            i += 1
        hit_file.close()
        hit_file = open(fold_path + file_name, 'w')
        for i in range(0, 5):
            hit_file.write(line_info[i] + "\n")
        hit_file.write(str(Max_num))
        hit_file.close()



# 从粗粒度实验BRAM模拟类型文件中获取BRAM写次数信息并建立词典
# opt = 1 int       opt = 0 float
def get_build_dict(file_path, opt):
    INT = 1
    FLOAT = 2
    bram_pos_dict = {}
    bram_file = open(file_path)
    for line in bram_file:
        if (len(line) > 1):
            line_ = line.split()
        tmp_pos = line_[0] + "_" + line_[1]
        if (opt == INT):
            bram_pos_dict[tmp_pos] = int(line_[2])
        if (opt == FLOAT):
            bram_pos_dict[tmp_pos] = float(line_[2])
    bram_file.close()
    return bram_pos_dict
# 从BRAM模拟单元中获取写次数 置换更新粗粒度实验的物理写率词典
def update_e_1_phy_dict(cur_limit, bram_pos_dict):
    phy_ratio_dict = {}
    for key in bram_pos_dict.keys():
        tmp_write_num = int(bram_pos_dict[key])
        if (tmp_write_num == 0):
            phy_ratio_dict[key] = 0
        else:
            tmp = tmp_write_num / cur_limit
            if (tmp > 1):
                phy_ratio_dict[key] = 1
            else:
                phy_ratio_dict[key] = tmp
    return phy_ratio_dict
# 更新phy信息
def update_e_1_phy(file_path, num_dict):
    file = open(file_path, 'w')
    for key in num_dict.keys():
        tmp_write_num = str(num_dict[key])
        tmp_str = key.replace("_", " ") + " " + tmp_write_num + "\n"
        file.write(tmp_str)
    file.close()



if (E == E_0):

    if (os.path.exists(cur_limit_path) == 1):
        print("continue\n")
    else:
        print("new test\n")


    times = 0
    sythesis_time = 0
    file = open(record_2_path, 'w')
    file.write("")
    file.close()
    E_limit = 0

    Effic_num = 0
    Effic = [0] * 10
    Effic_num_begin = 0
    E_flag = 0
    try_num = 0
    last_suc_flag = 1
    stage_ratio = 0


    cur_limit = ratio[stage_ratio]*MAX_WRITE_NUM



    while ( cur_limit < MAX_WRITE_NUM+1):
        record_1(cur_limit_path, cur_limit, times, sythesis_time, Effic_num_begin)
        if (last_suc_flag == 1):
            try_num = 0

        cmd = vpr + " " + xml + " " + blif + " " + nodisply +" "+ place + " "+route+ " " + route_opt + " " + seed + " " + str(try_num + 1) + " " + "***" + str(int(cur_limit))+"*"+str(int(100/min_ratio[stage_ratio])) + "  >" + benchmark_res_path + "vpr.out"
        print(cmd)
        os.system(cmd)
        flag = os.path.exists(route_file)

        if (flag == 1):
            cmd_del_f = "rm -rf " + route_file
            os.system(cmd_del_f)
            get_performance(res_path, performance_path, sythesis_time)
            last_suc_flag = 1
            old_tmies = times
            sythesis_time += 1
            Effic_num += 1

            cmd_transfrom = "python /home/zhlab/FPGA_NVBRAM/SRC/Simulator/S_transform.py " + E_path + " " + benchmark
            print(cmd_transfrom)
            os.system(cmd_transfrom)
            mv_place_pin = "mv " + benchmark_pre_info_src_path + benchmark + ".place_pin" + " " + benchmark_res_path + "place_pin/"
            mv_place_pin = mv_place_pin + str(sythesis_time) + ".place_pin"
            print(mv_place_pin)
            os.system(mv_place_pin)
            GET_BRAM_POS(benchmark_src_path, benchmark_pre_info_src_path, benchmark)
            pos_dict = {}
            BUILD_INIT_POS_DICT(pos_dict, benchmark_src_path, benchmark)
            inits = INIT_LIST()
            GET_ADDR_WRITE_NUM(inits, E_root_path + E_path + benchmark + "/src/" + benchmark + ".info_", E, amp_num)
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
                            break
                        if (line_.find("mem_name") != -1):
                            name_flag = 1
                    write_num.close()
                    BRAM_file_path = pos_dict[init_name]
                    print("Hit = " + str(init_) + "      BRAM_POS = " + BRAM_file_path)
                    BRAM_file_path = benchmark_BRAM_path + BRAM_file_path
                    command = update_path + " " + write_num_path + " " + BRAM_file_path + " " + str(int(cur_limit))
                    # print(command)
                    res = str(os.system(command))
                    print(res)
                    if (int(res) != 0):
                        print("Write_over")
                        update_Flag = 0

            new_tmies = times
            if (sythesis_time == 1 or E_flag == 1):
                E_limit = (new_tmies - old_tmies)
                E_flag = 0
                if (sythesis_time == 1):
                    Effic_num_begin = E_limit

            Effic[sythesis_time % 10] = new_tmies - old_tmies

            record_2(record_2_path, new_tmies - old_tmies, cur_limit)

            mv_place_cmd = "mv " + place_file + " " + place_res_path + str(sythesis_time) + ".place"
            print(mv_place_cmd)
            os.system(mv_place_cmd)

            record_1(cur_limit_path, cur_limit, times, sythesis_time, Effic_num_begin)
        else:
            last_suc_flag = 0
            try_num += 1
        if ((flag != 1 and try_num > 3)):  #
            stage_ratio += 1
            if (stage_ratio >= ratio_num):
                cur_limit = MAX_WRITE_NUM+3
            else:
                cur_limit = ratio[stage_ratio]*MAX_WRITE_NUM
                E_flag = 1
                Effic_num = 0
                try_num = 0

if (E == E_1):

    if (os.path.exists(cur_limit_path) == 1):
        print("continue\n")
    else:
        print("new test\n")


    times = 0
    sythesis_time = 0
    file = open(record_2_path, 'w')
    file.write("")
    file.close()
    E_limit = 0

    Effic_num = 0
    Effic = [0] * 10
    Effic_num_begin = 0
    E_flag = 0
    try_num = 0
    last_suc_flag = 1
    stage_ratio = 0


    cur_limit = MAX_WRITE_NUM
    continue_sythe_flag = 1



    bram_pos_dict = get_build_dict(e_1_bram_file_path, 1)

    while ( continue_sythe_flag == 1):
        record_1(cur_limit_path, cur_limit, times, sythesis_time, Effic_num_begin)
        phy_ratio_dict = update_e_1_phy_dict(cur_limit, bram_pos_dict)
        update_e_1_phy(phy_file_path, phy_ratio_dict)


        if (last_suc_flag == 1):
            try_num = 0

        cmd = vpr + " " + xml + " " + blif + " " + nodisply +" "+ place + " "+route+ " " + route_opt + " " + seed + " " + str(try_num + 1) + "  >" + benchmark_res_path + "vpr.out"
        print(cmd)
        os.system(cmd)
        flag = os.path.exists(route_file)

        if (flag == 1):
            cmd_del_f = "rm -rf " + route_file
            os.system(cmd_del_f)
            get_performance(res_path, performance_path, sythesis_time)
            last_suc_flag = 1
            old_tmies = times
            sythesis_time += 1
            Effic_num += 1

            GET_BRAM_POS(benchmark_src_path, benchmark_pre_info_src_path, benchmark)
            pos_dict = {}
            BUILD_INIT_POS_DICT(pos_dict, benchmark_src_path, benchmark)
            inits = INIT_LIST()
            GET_ADDR_WRITE_NUM(inits, E_root_path + E_path + benchmark + "/src/" + benchmark + ".info_", E_0, amp_num)
            update_Flag = 1
            transfrom_hit(write_num_path_pre)


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
                    if (init_write_num > 0):
                        BRAM_file_path = pos_dict[init_name]
                        BRAM_file_path = BRAM_file_path.replace("mem", "")
                        bram_pos_dict[BRAM_file_path] = bram_pos_dict[BRAM_file_path] + init_write_num
                        if (bram_pos_dict[BRAM_file_path] > cur_limit):
                            update_Flag = 0
                update_e_1_phy(e_1_bram_file_path , bram_pos_dict)

            mv_place_cmd = "mv " + place_file + " " + place_res_path + str(sythesis_time) + ".place"
            os.system(mv_place_cmd)
            new_tmies = times
            if (sythesis_time == 1 or E_flag == 1):
                E_limit = (new_tmies - old_tmies)
                E_flag = 0
                if (sythesis_time == 1):
                    Effic_num_begin = E_limit
            # Effic[sythesis_time % 10] = new_tmies - old_tmies
            record_2(record_2_path, new_tmies - old_tmies, cur_limit)
            record_1(cur_limit_path, cur_limit, times, sythesis_time, Effic_num_begin)
        else:
            last_suc_flag = 0
            try_num += 1

        if ((flag != 1 and try_num > 3)):  #
            continue_sythe_flag = 0
            try_num = 0
