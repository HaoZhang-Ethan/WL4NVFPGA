import numpy as np

import matplotlib.pyplot as plt


from pylab import *

def factorial(n):
    if n == 0 or n == 1:
        return 1
    else:
        return (n*factorial(n-1))

# print(factorial(15))

Max = 0
y1 = []
x1 = []
for j in range (15,20):

    x1.append(j)
    x = []
    for i in range(0,j):
        x.append(i)
    for x_ in x:
        y1_ = (factorial(j)/factorial(x_))*pow(2,x_)#
        if Max < y1_:
            Max = y1_
    y1.append(Max)
    # print("x = "+str(x_)+"\t"+str(y1_)+"\n")


# y1 = factorial(x)

plt.figure(1)


plt.plot(x1, y1)
plt.show()