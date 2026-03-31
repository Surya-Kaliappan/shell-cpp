#include <iostream>
#include <string>
#include <vector>
#include <cstdlib> // for getenv
#include <sstream> // for string stream
#include <unistd.h> // for access() and X_OK
#include <sys/wait.h> // for waitpid()

// funtion to split the command and arguments
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

// function to find the either type or executable file path
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

// function to execute external programs
void executeExternal(const std::string& cmd, const std::string& args_str) {
  std::vector<std::string> args_list;
  args_list.push_back(cmd);

  std::stringstream ss(args_str);
  std::string token;
  while(ss >> token) {
    args_list.push_back(token);
  }

  std::vector<char*> c_args;
  for(size_t i = 0; i < args_list.size(); i++) {
    c_args.push_back(const_cast<char*>(args_list[i].c_str()));
  }
  c_args.push_back(nullptr);

  pid_t pid = fork();
  
  if(pid == 0) {
    execvp(c_args[0], c_args.data()); // This will execute the command in child process
    std::cout << cmd << ": command not found\n"; // if the process success then this line won't work unless failed
    exit(1);
  } else if(pid > 0) {
    int status;  // this vaiable will hold the exit status of the child process
    waitpid(pid, &status, 0); // wait for the child process to finish
  } else {
    std::cerr << "Fork failed\n";
  }
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
      // std::cout << input << ": command not found" << std::endl;
      executeExternal(cmd, args);
    }

    std::cout << std::endl;
  }

  return 0;
}
