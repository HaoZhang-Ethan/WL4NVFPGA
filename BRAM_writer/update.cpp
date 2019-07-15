
#include "test.h"
#include "read_mem_file.h"
#include "read_adds_res.h"
#include "BRAM_write_unit.h"
#include "wirte_bram.h"
#include <cstdlib>
using namespace std;

long int option_char_to_long_int(char * test)
{
  long int temp = strtod(test,NULL);
  long int ftemp = atof(test);
  return ftemp;
}


int main(int argc, char* argv[])
{
    string bram_path = "";
    string new_bram_path = "";
    string adds_path = "";
    long int up_limit = 0;
    int flag = 0;       // 0 没有     1 超
    int tmp_flag = 0;
    if (argc<4)
    {
        cout<<"adds_path    bram_path   up_limit"<<endl;
    }
    else
    {
        adds_path = argv[1];
        bram_path = argv[2];
        up_limit = option_char_to_long_int(argv[3]);
        new_bram_path = bram_path;
        // cout<<"adds_path"<<adds_path<<endl;
        // cout<<"bram_path"<<bram_path<<endl;
        
        BRAMs brams =  get_brams(bram_path);               //获取全部mem_cell状态
        // BRAMs * p_brams = &brams;
        used_mem_info bram_info = get_bram_info(adds_path);  //获取单个BRAM写状态
        // used_mem_info * p_bram_info = &bram_info;
        BRAM_write_unit * p_write_unit_vector = buile_locat_map(bram_info.width);   //建立当前BRAM与MEM映射网络
        tmp_flag = update_mem(p_write_unit_vector,&bram_info ,&brams,up_limit);
        if (tmp_flag == 1)
        {
            flag = 1;
        }
        write_bram(new_bram_path,&brams);
        cout<<flag<<endl;
        return flag;
    }
}
