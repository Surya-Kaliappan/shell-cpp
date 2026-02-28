#include <iostream>
#include <string>

std::string getHead(std::string command) {
  return command.substr(0,command.find(" "));
}

std::string getBody(std::string command) {
  return command.substr(command.find(" ")+1);
}

void checkType(std::string command) {
  std::string arg = getBody(command);
  if(arg == "echo") {
    std::cout << "echo is a shell builtin";
  } else if (arg == "exit") {
    std::cout << "exit is a shell builtin";
  } else if (arg == "type") {
    std::cout << "type is a shell builtin";
  } else {
    std::cout << arg+": not found";
  }
  std::cout << std::endl;
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
