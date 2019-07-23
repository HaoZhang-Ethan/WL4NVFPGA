#-*-coding:utf-8-*-

import os


E_path = "s_run_e_2/"
benchmark = "LU8PEEng"
E_root_path = "/home/zhlab/BRAM/"
benchmark_src_path = E_root_path+E_path+benchmark+"/src/"
benchmark_res_path = E_root_path+E_path+benchmark+"/res/"
benchmark_pre_info_src_path = E_root_path+E_path+benchmark+"/src/pre_info_src/"
benchmark_BRAM_path = E_root_path+E_path+benchmark+"/res/BRAM/"
BRAM_path = E_root_path+E_path+benchmark+"/res/BRAM/e_2_bram"
folder_path = benchmark_src_path+"ace_pool/"
log_file_path = benchmark_pre_info_src_path + benchmark  + "_log.txt"
phy_file_path = benchmark_pre_info_src_path + benchmark  + "_phy.txt"
grid_path = "/home/zhlab/BRAM/SRC_07_09/FPGA_arch/60x60.arch"



MAX_INIT = 200
MAX_ADD_PIN = 15
class BRAM:
    def __init__(self):
        self.name = ""
        self.mode = ""
        self.dual = 0
        self.ratio = 0
        self.port_a_we = ""
        self.port_b_we = ""
        self.port_a_A = ["" for i in range(MAX_ADD_PIN )]
        self.port_b_A = ["" for i in range(MAX_ADD_PIN )]
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
def build_pin_dict(src_pach,folder_path):
    filenames = os.listdir(folder_path)
    pin_dict = {}
    #建立词频词典
    for filename in filenames:
        if (filename.find(".ace")!=-1):
            file_path = folder_path+filename
            file = open(file_path)
            for line in file:
                # print(line)
                line_ = line.split()
                pin_dict[line_[0]] = 0
            file.close()
            break
    iter_num = 0
    for filename in filenames:
        if (filename.find(".ace")!=-1):
            file_path = folder_path+filename
            file = open(file_path)
            for line in file:
                # print(line)
                line_ = line.split()
                tmp_str = line_[0]
                line_ = line_[1:]
                clk_num = len(line_)
                one_num = line_.count("1")
                # print(one_num)
                one_ratio = one_num/clk_num
                pin_dict[tmp_str] = pin_dict[tmp_str] + one_ratio
            iter_num += 1
            file.close()
    print(iter_num)
    for pin_dict_key in pin_dict.keys():
        pin_dict[pin_dict_key] = pin_dict[pin_dict_key]/iter_num

    return pin_dict



def get_log_init(log_file_path,pin_dict,brams):
    g_max = 0
    for it_bram in range(0,brams.num):
        if (brams.list[it_bram].dual == 0):
            if (len(brams.list[it_bram].port_a_we.replace(" ",""))>4):
                brams.list[it_bram].ratio = pin_dict[brams.list[it_bram].port_a_we]
            else:
                brams.list[it_bram].ratio = 0
            if (brams.list[it_bram].ratio > g_max):
                g_max = brams.list[it_bram].ratio
        else:
            port_a = brams.list[it_bram].port_a_we.replace(" ","")
            port_b = brams.list[it_bram].port_b_we.replace(" ","")
            max = 0
            if (len(port_a) > 4):
                tmp_ratio = pin_dict[port_a]
                if (tmp_ratio > max):
                    max = tmp_ratio
            if (len(port_b) > 4):
                tmp_ratio = pin_dict[port_b]
                if (tmp_ratio > max):
                    max = tmp_ratio
            brams.list[it_bram].ratio =  max
            if (max > g_max):
                g_max = max
    log_file = open(log_file_path,'w')
    for it_bram in range(0, brams.num):
        brams.list[it_bram].ratio = brams.list[it_bram].ratio/g_max
        log_file.write(brams.list[it_bram].name +" "+ str(brams.list[it_bram].ratio)+"\n")
    log_file.close()


def CREAT_BRAM_POS(grid_path,phy_file_path,BRAM_path):
    phy_file = open(phy_file_path,'w')
    bram_file = open(BRAM_path,'w')
    arr = []
    arc_read = open(grid_path)
    for line_ in arc_read:
        arr_ = []
        for col in range(len(line_) - 1):
            arr_.append(line_[col])
        arr.append(arr_)
    row_t = len(arr)
    col_t = len(arr[0])
    for i in range(0, col_t):
        flag = 1
        new_con = 0
        for j in range(0, row_t):
            if ((flag == 7 or flag == 1)):
                flag = 1
                new_con = j + 1
            if (arr[row_t - 1 - j][i] == "e"):
                str_tmp = str(i) + " " + str(new_con)+" 0\n"
                arr[row_t - 1 - j][i] = str_tmp
                if ((flag == 7 or flag == 2)):
                    phy_file.write(str_tmp)
                    bram_file.write(str_tmp)
            flag += 1
    bram_file.close()
    phy_file.close()


CREAT_INIT_INFO = 1
CREAT_PHY_POS = 2

STAGR = 2


if __name__=="__main__":
    if (STAGR == CREAT_INIT_INFO):
        pin_dict = build_pin_dict(benchmark_src_path,folder_path)
        brams = BRAMS()
        CREAT_INIT_INFO_FILE(benchmark_pre_info_src_path, benchmark, brams, 0)
        get_log_init(log_file_path, pin_dict, brams)
    if (STAGR == CREAT_PHY_POS):
        CREAT_BRAM_POS(grid_path,phy_file_path,BRAM_path)
