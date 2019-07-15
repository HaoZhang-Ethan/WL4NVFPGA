#-*-coding:utf-8-*-

import os

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

    pin_dict_path = src_pach + "pin_dict"
    pin_dict_file = open(pin_dict_path,'w')
    for pin in pin_dict.keys():
        tmp_str = pin + " " + str(pin_dict[pin]) + "\n"
        pin_dict_file.write(tmp_str)
    pin_dict_file.close()

if __name__=="__main__":
    src_pach = "/home/zhlab/BRAM/s_run/LU8PEEng/src/"
    folder_path = "/home/zhlab/BRAM/s_run/LU8PEEng/src/ace_pool/"
    build_pin_dict(src_pach,folder_path)