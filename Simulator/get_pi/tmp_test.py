import pandas as pd
import numpy as np
data = pd.read_csv(r'/home/zhlab/FPGA_NVBRAM/SRC/Simulator/get_pi/test.csv')
print(data.columns)#获取列索引值
data1  = data['flow']#获取列名为flow的数据作为新列的数据
data['cha1'] = data1 #将新列的名字设置为cha

data.to_csv(r"/home/zhlab/FPGA_NVBRAM/SRC/Simulator/get_pi/test.csv",index =False)
#mode=a，以追加模式写入,header表示列名，默认为true,index表示行名，默认为true，再次写入不需要行名
print(data)
