#include <iostream>
#include <string>
#include <cstdlib> // for getenv
#include <sstream> // for string stream
#include <unistd.h> // for access() and X_OK

std::string getHead(std::string command) {
  return command.substr(0,command.find(" "));
}

std::string getBody(std::string command) {
  return command.substr(command.find(" ")+1);
}

void checkType(std::string command) {
  std::string arg = getBody(command);
  if (arg == "echo" || arg == "exit" || arg == "type") {
    std::cout << arg << " is a shell builtin" << std::endl;
    return;
  }

  const char* path_env = std::getenv("PATH");
  if(path_env != nullptr) {
    std::stringstream ss(path_env);
    std::string dir;

    while(std::getline(ss, dir, ':')) {
      std::string full_path = dir + "/" + arg;
      if(access(full_path.c_str(), X_OK) == 0) {
        std::cout << arg << " is " << full_path << std::endl;
        return;
      }
    }
  }

  std::cout << arg << ": not found" << std::endl;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true){
    std::cout << "$ ";

    std::string command;
    std::getline(std::cin, command);
    if(command == "exit"){
      break;
    } else if (getHead(command) == "echo") {
      std::cout << command.substr(5) << std::endl;
    } else if (getHead(command) == "type") {
      checkType(command);
    } else {
      std::cout << command << ": command not found" << std::endl;
    }
  }
}
