#-*-coding:utf-8-*-



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


# benchmark_pre_info_src_path = sys.argv[3]
benchmark_pre_info_src_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/pre_info_src/"
# benchmark = sys.argv[4]
benchmark = "LU8PEEng"
# arch_file_path = sys.argv[5]


brams = BRAMS()
CREAT_INIT_INFO_FILE(benchmark_pre_info_src_path, benchmark, brams, 0)

# print("")

place_pin_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/pre_info_src/LU8PEEng.place_pin"
place_pin_file = open(place_pin_path)
tmp_bram = BRAMS()


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
# print("")
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

# print("");



benchmark_src_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/"

CREAT_INFO_FILE(brams,benchmark_src_path,benchmark)

