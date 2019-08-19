#include "wirte_bram.h"

void write_bram(string file,BRAMs * p_brams)
{
    int col = sizeof((*p_brams).Array[0])/sizeof((*p_brams).Array[0][0]);
    int row = sizeof((*p_brams).Array)/sizeof((*p_brams).Array[0]);

    ofstream outfile;
    outfile.open(file.data());
    outfile<<"info"<<endl;
    outfile<<"0"<<endl;
    outfile<<"use"<<endl;
    outfile<<"0"<<endl;
    outfile<<"mode"<<endl;
    outfile<<"0"<<endl;
    outfile<<"used_num"<<endl;
    outfile<<"0"<<endl;
    outfile<<"avg"<<endl;
    outfile<<"0"<<endl;
    outfile<<"other"<<endl;
    outfile<<"0"<<endl;
    outfile<<"other1"<<endl;
    outfile<<"0"<<endl;
    outfile<<"mat"<<endl;
    
    for(int i=0; i <row; ++ i)
        {
            for(int j=0; j <col; ++ j)
            {
            outfile<<((*p_brams).Array[i][j])<<" ";
            }
            outfile<<endl;
        }
        
    outfile.close();
}