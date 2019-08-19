
#include <iostream>
#include <string>
#include <fstream>
#include <cassert>
// #include <vector>
#include <sstream>
#include <cstring> 
using namespace std;
#define MAT 1
int main(int argc, char* argv[])
{
    string file = "";
    long int up_limit = 0;
    if (argc<3)
    {
        cout<<"string mem_file_path    long int up_limit"<<endl;
    }
    else
    {
    file = argv[1];
    stringstream stream;
    stream << argv[2];
    stream >> up_limit ;
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
                        // cout<<"___"<<file<<"___";
                        infile.close();
			cout<<"ERROR";
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
		  //cout<<"___"<<file<<"___";
                    infile.close(); 
		    cout<<"ERROR";
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
    cout<<"OK";
    return 0; 
    }
    
}
