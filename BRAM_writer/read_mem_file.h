#ifndef  READ_MEM_FILE_H
#define READ_MEM_FILE_H


#include <iostream>
#include <string>
#include <fstream>
#include <cassert>
#include <vector>
#include <sstream>
#include <cstring> 
using namespace std;


struct BRAMs
{
    long int Array[512][64];
};

BRAMs get_brams(string bram_path);
int trigger(string file,long int up_limit);

#endif