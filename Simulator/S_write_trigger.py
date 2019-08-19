#-*-coding:utf-8-*-

#-*-coding:utf-8-*-


CONF = 64
COL = 2
ROW = 256
MEM_COL = CONF
MEM_ROW = COL * ROW
MAX_WRITE_NUM = 10000             #BRAM的最大写次数
TEST_NUM = 2
MAX_ADD_PIN = 15
CLK = 5000

MAX_INIT = 200

# 根据net创建初始布局文件
class BRAM:
    def __init__(self):
        self.name = ""
        self.mode = ""
        self.dual = 0
        self.valid = 0
        self.port_a_we = ""
        self.port_b_we = ""
        self.port_a_A = ["" for i in range(15)]
        self.port_b_A = ["" for i in range(15)]
        self.port_c_A = ["" for i in range(15)]
class BRAMS:
    def __init__(self):
        self.num = 0
        self.list = []
        for i in range(0, MAX_INIT):  # 构造实例列表s
            self.list.append(BRAM())


def CREAT_INIT_INFO_FILE(benchmark_pre_info_src_path,benchmark,brams,Write):
    net_file_path = benchmark_pre_info_src_path+benchmark+".net"
    init_info_path = benchmark_pre_info_src_path + benchmark +"_init.info"
    net_file = open(net_file_path)
    flag = 0
    for line in net_file:
        if (flag == 1):
            # print(line)
            add_1_begin = line.find("<port name=\"addr1\">")
            add_2_begin = line.find("<port name=\"addr2\">")
            we_1_begin = line.find("<port name=\"we1\">")
            we_2_begin = line.find("<port name=\"we2\">")
            stop_flag = line.find("</inputs>")
            if(add_1_begin!=-1):
                brams.list[brams.num].port_a_A = line[add_1_begin+19:len(line)-8].split()
                # print(brams.list[brams.num].port_a_A)
            elif(add_2_begin!=-1):
                brams.list[brams.num].port_b_A = line[add_2_begin + 19:len(line) - 8].split()
                # print(brams.list[brams.num].port_b_A)
            elif(we_1_begin != -1):
                brams.list[brams.num].port_a_we = line[we_1_begin+17:len(line) - 8]
                # print(line[we_1_begin+17:len(line) - 8])
            elif(we_2_begin != -1):
                brams.list[brams.num].port_b_we = line[we_2_begin + 17:len(line) - 8]
                # print(line[we_2_begin + 17:len(line) - 8])
            elif(stop_flag!=-1):
                brams.num += 1
                flag = 0
        find_pos = line.find("mode=\"mem_")
        if (find_pos != -1):
            name_begin = line.find("name=\"")
            name_end = line.find("\" instance=")
            mode_end = line.find("\">")
            # brams.num += 1
            brams.list[brams.num].name = line[name_begin+6:name_end]
            brams.list[brams.num].mode = line[find_pos+6:mode_end]
            if (brams.list[brams.num].mode.find("dp")!= -1):
                brams.list[brams.num].dual = 1
            flag = 1
            # print(line[name_begin+6:name_end])
            # print(line[find_pos+6:mode_end])
    net_file.close()
    add_pin = set([])
    for i in range(brams.num):
        if (brams.list[i].dual == 0):
            if (len(brams.list[i].port_a_we)>4):
                add_pin.add(brams.list[i].port_a_we)
                for tmp_pin in brams.list[i].port_a_A:
                    if (len(tmp_pin) > 4 ):
                        add_pin.add(tmp_pin)

        else:
            if (len(brams.list[i].port_a_we)>4):
                add_pin.add(brams.list[i].port_a_we)
                for tmp_pin in brams.list[i].port_a_A:
                    if (len(tmp_pin) > 4 ):
                        add_pin.add(tmp_pin)
            if (len(brams.list[i].port_b_we)>4):
                add_pin.add(brams.list[i].port_b_we)
                for tmp_pin in brams.list[i].port_b_A:
                    if (len(tmp_pin) > 4 ):
                        add_pin.add(tmp_pin)
    if(Write == 1):
        init_info = open(init_info_path,'w')
        init_info.write("ALL_ADDRESS_BEGIN\n")
        for tmp_set in add_pin:
            init_info.write(tmp_set+"\n")
        init_info.write("ALL_ADDRESS_END")
        init_info.close()



def CREAT_INFO_FILE(brams,benchmark_src_path,benchmark):
    info_file_path = benchmark_src_path+benchmark+".info_"
    info_file = open(info_file_path,'w')
    info_file.write("INIT_BEGIN\n")
    for i in range(brams.num):
        if (brams.list[i].dual == 0):
            if (len(brams.list[i].port_a_we) > 4):
                info_file.write("INIT_NAME\n")
                info_file.write(brams.list[i].name+"\n")
                info_file.write("INIT_MODE\n")
                info_file.write(brams.list[i].mode + "\n")
                info_file.write("WE\n")
                info_file.write(brams.list[i].port_a_we+"\n")
                info_file.write("ADDRESS_BEGIN\n")
                counter = 0
                for tmp_pin in brams.list[i].port_a_A:
                    info_file.write("A[" + str(counter)+ "]=" + tmp_pin + "\n")
                    counter += 1
                info_file.write("ADDRESS_END\n")

        else:
            if (len(brams.list[i].port_a_we) > 4):
                info_file.write("INIT_NAME\n")
                info_file.write(brams.list[i].name+"\n")
                info_file.write("INIT_MODE\n")
                info_file.write(brams.list[i].mode + "\n")
                info_file.write("WE\n")
                info_file.write(brams.list[i].port_a_we+"\n")
                info_file.write("ADDRESS_BEGIN\n")
                counter = 0
                for tmp_pin in brams.list[i].port_a_A:
                    info_file.write("A[" +str(counter)+ "]=" + tmp_pin + "\n")
                    counter += 1
                info_file.write("ADDRESS_END\n")
            if (len(brams.list[i].port_b_we) > 4):
                info_file.write("INIT_NAME\n")
                info_file.write(brams.list[i].name+"\n")
                info_file.write("INIT_MODE\n")
                info_file.write(brams.list[i].mode + "\n")
                info_file.write("WE\n")
                info_file.write(brams.list[i].port_b_we+"\n")
                info_file.write("ADDRESS_BEGIN\n")
                counter = 0
                for tmp_pin in brams.list[i].port_b_A:
                    info_file.write("A["+str(counter)+"]="+tmp_pin+"\n")
                    counter += 1
                info_file.write("ADDRESS_END\n")
    info_file.write("INIT_END\n")
    info_file.close()



def transform(place_pin_path):
    tmp_bram = BRAMS()
    place_pin_file = open(place_pin_path)
    BRAM_counter = -1
    BRAM_num = 0
    valid = 0;
    flag_get_pin = -1
    for line in place_pin_file:
        line_ = line.split()
        if (flag_get_pin > 0 ):
            tmp_bram.list[BRAM_counter].port_c_A[valid - flag_get_pin] = line_[0]
            flag_get_pin -= 1

        if(line_[0] == "0"):
            BRAM_num = int(line_[2])
            continue

        if(line_[0] == "1"):
            BRAM_counter += 1
            tmp_bram.list[BRAM_counter].name = line_[2]
            continue
        if(line_[0] == "2"):
            tmp_bram.list[BRAM_counter].mode = line_[2]
            continue
        if(line_[0] == "3"):
            tmp_bram.list[BRAM_counter].dual = line_[2]
            continue
        if(line_[0] == "4"):
            tmp_bram.list[BRAM_counter].valid = int(line_[2])
            valid = int(line_[2])
            flag_get_pin = valid
            continue
    tmp_bram.num = BRAM_num
    pin_dict = {}
    for i in range(0,BRAM_num):
        name = tmp_bram.list[i].name
        for j in range(0,brams.num):
            if (brams.list[j].name == name):
                pin_dict.clear()
                for k in range(0,15):
                    pin_name = brams.list[j].port_a_A[k]
                    for x in range(0,tmp_bram.list[i].valid):
                        if(pin_name == tmp_bram.list[i].port_c_A[x]):
                            pin_dict[k] = x

                brams.list[j].port_a_A = tmp_bram.list[i].port_c_A.copy()
                if(brams.list[j].dual == 1):
                    brams.list[j].port_c_A = tmp_bram.list[i].port_c_A.copy()
                    for num_ in pin_dict.keys():
                        brams.list[j].port_c_A[pin_dict[num_]] = brams.list[j].port_b_A[num_]
                    brams.list[j].port_b_A = brams.list[j].port_c_A.copy()





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
    flag = 0
    init_num = 0
    for line in pos_file:
        line_ = line.split()
        if (len(line_)==3):
            # print(line_)
            mem_name = line_[1]+"_"+line_[2]+"mem"
            pos_dict[line_[0]] = mem_name






##
# '/home/zhlab/BRAM/vtr/vtr_ace_0/ace2/ace -b /home/zhlab/BRAM/s_run/or1200/src/pre_info_src/or1200.blif -n /home/zhlab/BRAM/s_run/or1200/src/pre_info_src/or1200_.blif -a /home/zhlab/BRAM/s_run/or1200/src/or1200_0.act>/home/zhlab/BRAM/s_run/or1200/res/or1200_0.out'
# 从抓取的电平文件中获取命中地址次数
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
    def my_init(self,dict_num):
        for i in range (dict_num):
            self.hit_add_dict[i] = 0

class INIT_LIST:
    def __init__(self):
        self.init_num = 0
        self.init_list = []
        for i in range (0,MAX_INIT):#构造实例列表 每个实例可以代表一个BRAM块 构造500块
            self.init_list.append(INIT())

# 构建bit词典    1024-10 512-9
BIT_DICT = {}
for i in range(0,MAX_ADD_PIN+1):
    BIT_DICT[pow(2,i)]= i


def GET_ADDR_WRITE_NUM(inits):
    info_file_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/LU8PEEng.info_"
    add_pre_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/ace_pool/"
    addr_hit_num_path = "/home/zhlab/BRAM/s_run/LU8PEEng/res/address/"
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




#检测是否写到上限
def trigger(benchmark_BRAM_path,trigger_path,pos_dict):
    for mem in pos_dict.values():
        mem_file_path = benchmark_BRAM_path + mem
        command_trigger = trigger_path +" " + mem_file_path +" "+ "200000"
        # print(command_trigger)
        res = str(os.system(command_trigger))
        if (len(res) > 2):
            return 0
    return 1


#
benchmark_pre_info_src_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/pre_info_src/"
benchmark = "LU8PEEng"
place_pin_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/pre_info_src/LU8PEEng.place_pin"
benchmark_src_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/"
brams = BRAMS()
CREAT_INIT_INFO_FILE(benchmark_pre_info_src_path, benchmark, brams, 0)
transform(place_pin_path)
CREAT_INFO_FILE(brams,benchmark_src_path,benchmark)

#

#
#
#
# #
# # GET_BRAM_POS(benchmark_src_path, benchmark_pre_info_src_path,benchmark)
# # pos_dict = {}
# # BUILD_INIT_POS_DICT(pos_dict, benchmark_src_path, benchmark)
#
#
#
#
# benchmark_res_path = "/home/zhlab/BRAM/s_run/LU8PEEng/res"
# inits = INIT_LIST()
# GET_ADDR_WRITE_NUM(inits)
#
# # run_time = 0
# # while(trigger(benchmark_BRAM_path, trigger_path, pos_dict)):
# #     CREAT_RAND_ACT(get_act_path, benchmark_res_path, TEST_NUM, benchmark, benchmark_pre_info_src_path)
# #     CARLL_ACE(benchmark_src_path, benchmark, benchmark_res_path, benchmark_pre_info_src_path, ace_path, TEST_NUM)
# #     inits = INIT_LIST()
# #     GET_ADDR_WRITE_NUM(benchmark_src_path, benchmark_res_path, benchmark, inits)
# #     for init_ in range(inits.init_num):
# #         write_num_path = write_num_path_pre+str(init_)
# #         write_num = open(write_num_path)
# #         name_flag = 0
# #         init_name = ""
# #         for line_ in write_num:
# #             if(name_flag == 1):
# #                 init_name = line_.replace("\n","")
# #                 break
# #             if(line_.find("mem_name")!=-1):
# #                 name_flag = 1
# #         write_num.close()
# #         BRAM_file_path = pos_dict[init_name]
# #         BRAM_file_path = benchmark_BRAM_path + BRAM_file_path
# #         command = update_path +" " + write_num_path + " " +BRAM_file_path
# #         # print(command)
# #         os.system(command)
# #         run_time += 1
# # print(run_time)