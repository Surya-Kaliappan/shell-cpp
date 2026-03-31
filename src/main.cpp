#include <iostream>
#include <string>
#include <cstdlib> // for getenv
#include <sstream> // for string stream
#include <unistd.h> // for access() and X_OK

void parseCommand(const std::string& input, std::string& cmd, std::string& args) {
  size_t space_pos = input.find(' ');

  if(space_pos != std::string::npos) {
    cmd = input.substr(0, space_pos);
    args = input.substr(space_pos+1);
  } else {
    cmd = input;
    args = "";
  }
}

void checkType(const std::string& args) {
  if(args.empty()) return ;
  
  if(args == "echo" || args == "exit" || args == "type") {
    std::cout << args << " is a shell builtin\n";
    return ;
  }

  const char* path_env = std::getenv("PATH");
  if(path_env != nullptr) {
    std::stringstream ss(path_env);
    std::string dir;

    while(std::getline(ss, dir, ':')) {
      std::string full_path = dir + "/" + args;
      if(access(full_path.c_str(), X_OK) == 0) {
        std::cout << args << " is " << full_path << "\n";
        return ;
      }
    }
  }

  std::cout << args << ": not found\n";
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string input;

  while(true){
    std::cout << "$ ";

    if(!std::getline(std::cin, input)) {
      break;
    }
    if(input.empty()) continue; // Ignore empty Enter presses

    std::string cmd, args;
    parseCommand(input, cmd, args);
    if(cmd == "exit"){
      break;
    } else if (cmd == "echo") {
      std::cout << args << std::endl;
    } else if (cmd == "type") {
      checkType(args);
    } else {
      std::cout << input << ": command not found" << std::endl;
    }
  }

  return 0;
}
