#include "BRAM_write_unit.h"
BRAM_write_unit *  buile_locat_map(int width )
{
    int row_num = CONFIG_WIDTH / width;
    int address_range = ((ROW_DEPTH*COL_WIDTH*CONFIG_WIDTH) / width);
    BRAM_write_unit * p_write_unit_vector;
    p_write_unit_vector = (BRAM_write_unit *)malloc(address_range*sizeof(BRAM_write_unit));
    int tmp_i ;
    int tmp_j ;
    int tmp_row = 0;
    int tmp_col = 0;
    int tmp_row_num = 0;
    for (tmp_i = 0; tmp_i < address_range ;tmp_i++)
        {
            if (tmp_row_num == row_num)
            {
                tmp_row++;
                tmp_row_num = 0;
            }
            (*(p_write_unit_vector+tmp_i)).unit_num = width;
            (*(p_write_unit_vector+tmp_i)).address = tmp_i;
            (*(p_write_unit_vector+tmp_i)).dirty = false;
            // (*(p_write_unit_vector+tmp_i)).write_num = 0;
            (*(p_write_unit_vector+tmp_i)).relocation_table_x = tmp_row;
            int begin_j =  width * tmp_row_num;
            int tmp_y = 0;
            for (tmp_j = begin_j;tmp_j < (begin_j+width) ;tmp_j++)
                {
                    (*(p_write_unit_vector+tmp_i)).relocation_table_y[tmp_y++] = tmp_j; 
                }
            tmp_row_num++;
        }
    return p_write_unit_vector;
}

int update_mem(BRAM_write_unit * p_wirte_unit,used_mem_info * p_mem_info,BRAMs * p_brams,long int up_limit)
{
    int write_num = (*p_mem_info).used_depth;             //单个BRAM使用的地址
    int tmp_i,tmp_j,tmp_k;
    int flag = 0;                                           //标记单个CELL是否写超    0 没有      1 超
    for (tmp_i = 0 ; tmp_i < write_num;tmp_i++)
    {
        int address = (*p_mem_info).Array[tmp_i][0];
        int tmp_row = (*(p_wirte_unit+address)).relocation_table_x;
        for (tmp_j = 0;tmp_j<(*(p_wirte_unit+address)).unit_num;tmp_j++)
        {
            int tmp_col = (*(p_wirte_unit+address)).relocation_table_y[tmp_j];
            // cout<<(*p_brams).Array[tmp_row][tmp_col]<<"  "<<(*p_mem_info).Array[tmp_i][1]<<endl;
            (*p_brams).Array[tmp_row][tmp_col] += (*p_mem_info).Array[tmp_i][1];
            if((*p_brams).Array[tmp_row][tmp_col] > up_limit)
            {
                flag = 1;
            }
        }
    }
    return flag;
}