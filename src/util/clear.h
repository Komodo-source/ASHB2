#ifndef CLEAR_H
#define CLEAR_H

#include <filesystem>
#include <iostream>
#include <string>
#include <cstdio>

void rm_data_file(){
  try{
    int i = 0;
    while(true){
      std::string file = "./src/data/" + std::to_string(i) + ".csv";
      if (std::remove(file.c_str()) == 0){
        i++;
        //std::cout << "file " << "./src/data/" + std::to_string(i) + ".csv" << " deleted.\n";
      }else{
        //std::cout << "./src/data/act_" + std::to_string(i) + ".csv""d";
        break;
      }
    }
  }catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << '\n';
    }
}

void rm_data_act_file(){
  try{
    int i = 0;
    while(true){
      std::string file = "./src/data/act_" + std::to_string(i) + ".csv";
      if (std::remove(file.c_str()) == 0){
       //std::cout << "file " << "./src/data/act_" + std::to_string(i) + ".csv" << " deleted.\n";
       i++;
      }else{
        break;
      }
    }
  }catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << '\n';
    }
}

void rm_log_files(){
  try{
    std::vector<std::string> logFiles = {
      "./src/data/cmd_log.txt",
      "./src/data/deaths_log.txt",
      "./src/data/diseases_log.txt",
      "./src/data/actions_log.txt",
      "./src/data/relationships_log.txt",
      "./src/data/movements_log.txt",
      "./src/data/births_log.txt",
      "./src/data/events_log.txt",
      "./src/data/tick_history.jsonl"
    };

    for(const auto& file : logFiles){
      std::remove(file.c_str());
    }
    std::cout << "remove_logs \n";
  }catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << '\n';
    }
}

#endif
