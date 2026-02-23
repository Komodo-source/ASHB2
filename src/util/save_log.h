#include <iostream>
#include <string>
#include <fstream>

  void save_log(std::string val, std::string file_name){
    std::ifstream myfile(file_name);

    while (std::getline (myfile, val)) {
      // Output the text from the file
      std::cout << val;
    }

    myfile.close();
  }

