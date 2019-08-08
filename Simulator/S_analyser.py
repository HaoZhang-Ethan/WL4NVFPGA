#-*-coding:utf-8-*-

import os


CLK = 5000
pin_num = 300




# Pin_set
MAX_INIT = 200
MAX_ADD_PIN = 15

class BRAM:
    def __init__(self):
        self.name = ""
        self.mode = ""
        self.dual = 0
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

def if_valid(str_name):
    str_name_tmp = str_name.lower()
    if(len(str_name_tmp)>4):
        return 1
    else:
        return 0






    # for It_bram in range (0,brams.num):
    #     if brams.list[It_bram].dual == 0:
    #         we_line = brams.list[It_bram].port_a_we
    #         if (if_valid(we_line)==2):

def build_pin_dict(src_pach,folder_path,brams):

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

    pin_dict_path = src_pach + "pin_dict"
    pin_dict_file = open(pin_dict_path,'w')
    for pin in pin_dict.keys():
        tmp_str = pin + " " + str(pin_dict[pin]) + "\n"
        pin_dict_file.write(tmp_str)
    pin_dict_file.close()




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

def Read_CLK_ARRAAY(folder_path,brams):
    filenames = os.listdir(folder_path)
    pin_dict_flag = {}
    pin_dict = {}
    pin_dict_find = {}
    #建立词频词典
    for filename in filenames:
        if (filename.find(".ace")!=-1):
            file_path = folder_path+filename
            file = open(file_path)
            pin_num_it = 0
            for line in file:
                line_ = line.split()
                pin_dict_flag[line_[0]] = 0
                pin_dict[line_[0]] = 0
                pin_dict_find[line_[0]] = pin_num_it
                pin_num_it += 1
            file.close()
            break
    iter_num = 0
    for filename in filenames:

        if (filename.find(".ace")!=-1):
            CLK_ARRAY = [[0] * CLK] * pin_num
            file_path = folder_path+filename
            file = open(file_path)
            pin_num_it = 0
            for line in file:
                line_ = line.split()
                line_ = line_[1:]
                CLK_ARRAY[pin_num_it] = line_
                pin_num_it += 1
            iter_num += 1
            file.close()

            for pin_dict_flag_ in pin_dict_flag.keys():
                pin_dict_flag[pin_dict_flag_] = 0

            for bram_it in range (0,brams.num):
                if (brams.list[bram_it].dual == 0):
                    we_line = brams.list[bram_it].port_a_we.replace(" ","")
                    for pin_it in range(0,15):
                        pin_name = brams.list[bram_it].port_a_A[pin_it].replace(" ","")
                        if (if_valid(pin_name) == 1 and pin_dict_flag[pin_name] == 0):
                            pin_dict_flag[pin_name] = 1
                            for clk_ in range(0,CLK):
                                if ((CLK_ARRAY[pin_dict_find[pin_name]][clk_] == '1') and (CLK_ARRAY[pin_dict_find[we_line]][clk_] == '1') ):
                                    pin_dict[pin_name] += 1
                else:
                    we_line_a = brams.list[bram_it].port_a_we.replace(" ","")
                    we_line_b = brams.list[bram_it].port_b_we.replace(" ","")
                    if (if_valid(we_line_a) == 1):
                        for pin_it in range(0, 15):
                            pin_name = brams.list[bram_it].port_a_A[pin_it].replace(" ","")
                            if (if_valid(pin_name) == 1 and pin_dict_flag[pin_name] == 0):
                                pin_dict_flag[pin_name] = 1
                                for clk_ in range(0 , CLK):
                                    if ((CLK_ARRAY[pin_dict_find[pin_name]][clk_] == '1') and (CLK_ARRAY[pin_dict_find[we_line_a]][clk_] == '1')):
                                        pin_dict[pin_name] += 1
                    if (if_valid(we_line_b) == 1):
                        for pin_it in range(0, 15):
                            pin_name = brams.list[bram_it].port_b_A[pin_it].replace(" ","")
                            if (if_valid(pin_name) == 1 and pin_dict_flag[pin_name] == 0):
                                pin_dict_flag[pin_name] = 1
                                for clk_ in range(0 , CLK):
                                    if ((CLK_ARRAY[pin_dict_find[pin_name]][clk_] == '1') and (CLK_ARRAY[pin_dict_find[we_line_b]][clk_] == '1')):
                                        pin_dict[pin_name] += 1
    return pin_dict


def write_dict(pin_dict,src_pach):

    pin_dict_path = src_pach + "pin_dict"
    pin_dict_file = open(pin_dict_path,'w')
    for pin in pin_dict.keys():
        tmp_str = pin + " " + str(pin_dict[pin]) + "\n"
        pin_dict_file.write(tmp_str)
    pin_dict_file.close()



if __name__=="__main__":
    E_path = "s_run/"

    # benchmark = "boundtop"
    # benchmark = "LU8PEEng"
    # benchmark =  "LU32PEEng"
    benchmark =  "mcml"
    # benchmark =  "mkDelayWorker32B"
    # benchmark =  "mkPktMerge"
    # benchmark =  "mkSMAdapter4B"
    # benchmark = "or1200"
    src_pach = "/home/zhlab/BRAM/s_run/"+benchmark+"/src/"
    folder_path = "/home/zhlab/BRAM/s_run/"+benchmark+"/src/ace_pool/"
    benchmark_pre_info_src_path = "/home/zhlab/BRAM/" + E_path + benchmark + "/src/pre_info_src/"

    brams = BRAMS()
    CREAT_INIT_INFO_FILE(benchmark_pre_info_src_path, benchmark, brams, 0)


    pin_dict = Read_CLK_ARRAAY(folder_path,brams)
    for pin_dict_key in pin_dict.keys():
        tmp = pin_dict[pin_dict_key]
        pin_dict[pin_dict_key] = tmp/(1000*CLK)

    write_dict(pin_dict, src_pach)

    # build_pin_dict(src_pach,folder_path,brams)