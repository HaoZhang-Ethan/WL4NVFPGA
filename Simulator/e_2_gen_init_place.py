#-*-coding:utf-8-*-

import os

file_path = "/home/zhlab/BRAM/e_2/B4/src/pre/B4.place"
init_path = file_path[0:len(file_path)-6] + "_init.place"

vec = []
vec.clear()

file = open(file_path)

for file_line in file:
    file_line_ = file_line.split()
    if(len(file_line_) == 5 ):
        vec.append(file_line_[0:4])

file.close()
print(len(vec))
init_file = open(init_path,'w')

init_file.write(str(len(vec)-1)+'\n')
for i in range(1,len(vec)):
    tmp_str = vec[i][0] + " "+vec[i][1]+" "+vec[i][2]+" "+vec[i][3]+"\n"
    init_file.write(tmp_str )

init_file.close()
