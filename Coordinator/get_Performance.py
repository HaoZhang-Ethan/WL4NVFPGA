#-*-coding:utf-8-*-
import os



def record_performance(file_path,line):
    file = open(file_path, 'a')
    file.write(line)
    file.close()




if __name__=="__main__":

    res_path = "/home/zhlab/BRAM/vtr/vtr_release/vpr/5.txt"
    performance_path = "/home/zhlab/BRAM/vtr/vtr_release/vpr/6.txt"

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

    line_info = str(t_logic_delay) + "\t" + str(t_net_delay)+"\t"+str(critical_path)+"\n"
    "\t" + critical_path
    record_performance(performance_path, line_info)