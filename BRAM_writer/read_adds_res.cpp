
#include "read_adds_res.h"

#define READ_BEGIN 1
#define MEM_MODE 2


// struct used_mem_info
// {
//     int depth = 0;
//     int width = 0;
//     int used_depth = 0;
//     long int Array[32768][2];
// };


used_mem_info readTxt_adds(string file,long int * p_Array)
{
    ifstream infile; 
    infile.open(file.data());   //将文件流对象与文件连接起来 
    assert(infile.is_open());   //若失败,则输出错误消息,并终止程序运行 

    used_mem_info info;
    // vector<long int> res; 
    string s;
    int flag = 0;
    int Row_array = 0;
    int Col_array = 0;
    int depth = 0;
    int width = 0;
    while(getline(infile,s))
    {
        if (flag == READ_BEGIN )
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
                    *(p_Array+2*Row_array+Col_array) = number;
                    Col_array ++;
                    // res.push_back(number);
                    s.clear();
                    break;
                }
                string s_temp = s.substr(0, s.find(" "));
                stringstream stream;
                long int number = 0;
                stream << s_temp;
                stream >> number ;
                stream.clear();
                *(p_Array+2*Row_array+Col_array) = number;
                Col_array ++;
                // res.push_back(number);
                s.erase(0, s.find(" ") + 1); 
            }
            Row_array ++;
        }
        if (flag == MEM_MODE )
        {
            // cout<<s<<endl;
            // cout<<s.find("x")<<endl;
            // cout<<s.rfind("_")<<endl;
            stringstream stream;
            stream << s.substr(s.find("_")+1,(s.rfind("x")-s.find("_")-1));
            stream >> depth;
            stream.clear();
            stream <<  s.substr(s.find("x")+1,(s.rfind("_")-s.find("x")-1));
            stream >> width;
            stream.clear();
            // cout<<s.substr(s.find("_")+1,(s.rfind("x")-s.find("_")-1))<<endl;
            // cout<<s.substr(s.find("x")+1,(s.rfind("_")-s.find("x")-1))<<endl;
            // cout<<s<<endl;
            info.depth = depth;
            info.width = width;
            flag = 0;
        }
        if (s == "add_begin")
        {
            flag = READ_BEGIN;
        }
        if (s == "mem_mode")
        {
            flag = MEM_MODE;
        }
    }
    info.used_depth = Row_array;
    infile.close();   
    return info; 
}

void zh()
{
    cout<<"zzzzz"<<endl;
}

used_mem_info get_bram_info(string adds_path)
{
    long int Array[32768][2];
    used_mem_info info = readTxt_adds(adds_path,(long int*)Array);
    memcpy(info.Array,Array,sizeof(Array));
    return info;
}


