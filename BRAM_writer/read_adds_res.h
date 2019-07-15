
#ifndef  READ_ADDRESS_RES_H
#define READ_ADDRESS_RES_H

#include <iostream>
#include <string>
#include <fstream>
#include <cassert>
#include <vector>
#include <sstream>
#include <cstring> 
using namespace std;

struct used_mem_info
{
    int depth;
    int width;
    int used_depth;
    long int Array[32768][2];
};


used_mem_info readTxt(string file,long int * p_Array);
used_mem_info get_bram_info(string adds_path);
void zh();
// used_mem_info get_addr_info();



 
#endif