#include <iostream>
#include <vector>
#include <map>

/*
         foreground background
black        30         40
red          31         41
green        32         42
yellow       33         43
blue         34         44
magenta      35         45
cyan         36         46
white        37         47
*/


  void debbug_log(auto val, int col=37){
    std::cout << "\033[1;31m red text\033[0m\n";
    std::cout << "\033[1;" << col << "m" <<  val << "\033[0m\n";
  }

  void print_vector_int(std::vector<int> v, int col=37){
    for(int i : v){
      std::cout  << i << ", ";
    }
    std::cout << "\n";
  }

  void print_vector_str(std::vector<char*> v, int col=37){
    for(char* i : v){
      std::cout  << i << ", ";
    }
    std::cout << "\n";
  }

