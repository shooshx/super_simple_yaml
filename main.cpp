#include "ss_yaml.hpp"

#include <fstream>
#include <iostream>
#include <Windows.h>

using namespace std;

const char *test1 = R"**(
version: 8
str1: aaaaaa
str2 : aa3334
lst_emp1: [ ]
lst_emp2: []
num1: 1.2e-3
lst1:
   - 123
   - 456
   - 
     - 1.2
     - 2.3
   - [1,2,3,4]
inmap:
   aa: 67
   bb: -89
   cc:
      a: -bla
      b: kxla
   dd: 8 
)**";

const char *test2 = "- bla";

// tbd quotes


int main()
{


    ifstream ifs("C:/projects/ss_yaml/test2.yml");
    ifs.seekg(0, ios::end);
    int sz = (int)ifs.tellg();
    ifs.seekg(0, ios::beg);
    vector<char> buf(sz+1);
    ifs.read(&buf[0], sz);
    buf[sz] = 0;

    auto start = GetTickCount();
    for (int i = 0; i < 10; ++i) {
        ss_yaml::Yaml doc;
        doc.parse(&buf[0]);
    }
    auto elapsed = GetTickCount() - start;
    cout << "done " << elapsed << endl;

    //auto s = doc.root()["objects"][1]["data"]["model_data"]["height"].dbl();


    return 0;
}