#include "ss_yaml.hpp"


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
    ss_yaml::Yaml doc;
    doc.parse(test1);

    return 0;
}