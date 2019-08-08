#-*-coding:utf-8-*-

import os
import random

# 对BRAM进行预先写入



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


# benchmark = "boundtop"
benchmark =  "LU8PEEng"
# benchmark =  "LU32PEEng"
# benchmark =  "mcml"
# benchmark =  "mkDelayWorker32B"
# benchmark =  "mkPktMerge"
# benchmark =  "mkSMAdapter4B"

#BRAM_log
# 创建BRAM   x_ymem初始化文件
def new_BRAM_file(benchmark_BRAM_path,name,MEM_ROW,MEM_COL):
        BRAM_file_path = benchmark_BRAM_path + name + "mem"
        BRAM_file = open(BRAM_file_path, 'w');
        pro_info = "info\nuse\n0\nmode\n0\nused_num\n0\navg\n0\nother\n0\nother1\n0\nmat\n";
        BRAM_file.write(pro_info);
        for i in range(MEM_ROW):
            str_ = ""
            for j in range(MEM_COL):
                num = random.randint(0, 999)
                str_ += str(num)+" ";
            str_ += "\n";
            BRAM_file.write(str_);
        BRAM_file.close();
# 产生BRAM_log文件
def CREAT_BRAM_FILE(grid_path,benchmark_BRAM_path,MEM_COL,MEM_ROW):
    arr = []
    arc_read = open(grid_path)
    row = 0
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
                str_tmp = str(i) + "_" + str(new_con)
                arr[row_t - 1 - j][i] = str_tmp
                if ((flag == 7 or flag == 2)):
                    new_BRAM_file(benchmark_BRAM_path,str_tmp,MEM_ROW,MEM_COL)
            flag += 1

if __name__=="__main__":
    BRAM_log = 1
    STAGR = 1
    if(STAGR == BRAM_log):# 初始化BRAM文件
        # grid_path = sys.argv[2]
        grid_path = "/home/zhlab/BRAM/SRC_07_09/FPGA_arch/100x100.arch"
        # benchmark_BRAM_path = sys.argv[3]
        benchmark_BRAM_path = "/home/zhlab/BRAM/"+E_path+benchmark+"/res/BRAM/"
        # MEM_COL = int(sys.argv[4])
        MEM_COL = 64
        # MEM_ROW = int(sys.argv[5])
        MEM_ROW = 512

        # print(grid_path)
        # print(benchmark_BRAM_path)
        # print(MEM_COL)
        # print(MEM_ROW)
        CREAT_BRAM_FILE(grid_path, benchmark_BRAM_path, MEM_COL, MEM_ROW)
        # print("END")