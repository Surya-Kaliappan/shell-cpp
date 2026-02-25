#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true){
    std::cout << "$ ";

    std::string command;
    std::getline(std::cin, command);
    if(command == "exit"){
      break;
    } else if (command.substr(0,command.find(" ")) == "echo") {
      std::cout << command.substr(command.find(" ")+1) << std::endl;
    } else {
      std::cout << command << ": command not found" << std::endl;
    }
  }
}
