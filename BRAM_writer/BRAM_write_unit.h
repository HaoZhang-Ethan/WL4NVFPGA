#include <iostream>
#include <malloc.h>
#include "read_adds_res.h"
#include "read_mem_file.h"

#define CONFIG_WIDTH 64
#define COL_WIDTH 2
#define ROW_DEPTH 256
using namespace std;


struct BRAM_write_unit
{
    int address;
    int unit_num;
    bool dirty;
    int relocation_table_x;
    int relocation_table_y[64];
    // int write_num;              //写次数
};

BRAM_write_unit *  buile_locat_map( int width );
int update_mem(BRAM_write_unit * p_wirte_unit, used_mem_info * mem_info, BRAMs * brams,long int up_limit);