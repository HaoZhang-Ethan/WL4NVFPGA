
#include "read_mem_file.h"

#define MAT 1





void readTxt_mem(string file,long int * p_Array)
{
    ifstream infile; 
    infile.open(file.data());   //将文件流对象与文件连接起来 
    assert(infile.is_open());   //若失败,则输出错误消息,并终止程序运行 
    string s;
    int flag = 0;
    int Row_array = 0;
    int Col_array = 0;
    int depth = 0;
    int width = 0;
    while(getline(infile,s))
    {
        if (flag == MAT )
        {
            Col_array = 0;
            // cout<<s<<endl;
             while (!s.empty())
            {
                if (s.find(" ") == string::npos) 
                {

                    stringstream stream;
                    long int number = 0;
                    stream << s;
                    stream >> number ;
                    stream.clear();
                    *(p_Array+64*Row_array+Col_array) = number;
                    // cout<<number<<endl;
                    Col_array ++;
                    s.clear();
                    break;
                }
                string s_temp = s.substr(0, s.find(" "));
                stringstream stream;
                long int number = 0;
                stream << s_temp;
                stream >> number ;
                stream.clear();
                // cout<<number<<endl;
                *(p_Array+64*Row_array+Col_array) = number;
                Col_array ++;
                s.erase(0, s.find(" ") + 1); 
            }
            Row_array ++;
        }
        
        if (s == "mat")
        {
            flag = MAT;
        }
    }
    infile.close();   

}


BRAMs get_brams(string bram_path)
{
    BRAMs brams;
    long int Array[512][64];
    memset(Array,0,sizeof(Array));
    readTxt_mem(bram_path,(long int*)Array);
    memcpy(brams.Array,Array,sizeof(Array));
    return brams;
}


int trigger(string file,long int up_limit)
{
    ifstream infile; 
    infile.open(file.data());   //将文件流对象与文件连接起来 
    assert(infile.is_open());   //若失败,则输出错误消息,并终止程序运行 
    string s;
    int flag = 0;
    int Row_array = 0;
    int Col_array = 0;
    int depth = 0;
    int width = 0;
    while(getline(infile,s))
    {
        if (flag == MAT )
        {
            // Col_array = 0;
            // cout<<s<<endl;
             while (!s.empty())
            {
                if (s.find(" ") == string::npos) 
                {

                    stringstream stream;
                    long int number = 0;
                    stream << s;
                    stream >> number ;
                    stream.clear();
                    if (number > up_limit)
                    {
                        return 1;
                    }
                    // cout<<"--->"<<number<<endl;
                    s.clear();
                    break;
                }
                string s_temp = s.substr(0, s.find(" "));
                stringstream stream;
                long int number = 0;
                stream << s_temp;
                stream >> number ;
                stream.clear();
                if (number > up_limit)
                {
                    return 1;
                }
                // cout<<number<<endl;
                s.erase(0, s.find(" ") + 1); 
            }
        }
        
        if (s == "mat")
        {
            flag = MAT;
        }
    }
    infile.close();  
    return 0; 
}