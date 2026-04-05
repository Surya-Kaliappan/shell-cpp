#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <sstream> // for string stream
#include <unistd.h> // for access() and X_OK and getcwd()
#include <sys/wait.h> // for waitpid()

// funtion to split the command and arguments and respect quotes
std::vector<std::string> parseInput(const std::string& input) {
  std::vector<std::string> args;
  std::string current;
  bool in_single = false;
  bool in_double = false;
  bool in_word = false;  // track if we have started building a word

  for(size_t i=0; i<input.length(); i++) {
    char c = input[i];

    if(c == '\\' && !in_single && !in_double) {  // Backslash in outside quotes
      if(i+1 < input.length()) {
        current += input[i+1];
        in_word = true;
        i++;
      }
    }
    else if(c == '\'' && !in_double) {
      in_single = !in_single; // Toggle state
      in_word = true; // Even empty quotes like '' count as a word
    } else if (c == '"' && !in_single) {
      in_double = !in_double;
      in_word = true;
    } else if (c == ' ' && !in_single && !in_double) {
      // Space OUTSIDE of quotes means the word is done
      if(in_word) { 
        args.push_back(current);
        current.clear();
        in_word = false;
      }
    } else {
      // Any other character gets added including the space inside the quotes
      current += c;
      in_word = true;
    }
  }
  if(in_word) {
    args.push_back(current); // pushing the final word if string ended
  }

  return args;
}

// function to find the either type or executable file path
void checkType(const std::string& args) {
  if(args.empty()) return ;

  std::unordered_set<std::string> builtins = {
    "echo", "exit", "type", "pwd", "cd"
  };

  if(builtins.find(args) != builtins.end()) {
    std::cout << args << " is a shell builtin\n";
    return ;
  }

  const char* path_env = std::getenv("PATH");
  if(path_env != nullptr) {
    std::stringstream ss(path_env);
    std::string dir;

    while(std::getline(ss, dir, ':')) {
      std::string full_path = dir + "/" + args;
      if(access(full_path.c_str(), X_OK) == 0) {  // X_OX means check the executable file
        std::cout << args << " is " << full_path << "\n";
        return ;
      }
    }
  }

  std::cout << args << ": not found\n";
}

void executeEcho(const std::vector<std::string>& tokens) {
  for(size_t i=1; i<tokens.size(); i++) {
    std::cout << tokens[i] << (i == tokens.size()-1 ? "" : " ");
  }
  std::cout << "\n";
}

void executePwd() {
  char cwd[1024]; // buffer to hold the current working directory path
  if(getcwd(cwd, sizeof(cwd)) != nullptr) {  // char *getcwd(char *buf, size_t size);
    std::cout << cwd << "\n";
  } else {
    std::cerr << "Error: Could not get current directory\n";
  }
}

void executeCd(std::string path) {
  if(path.empty()) return;

  if (path[0] == '~') {
        const char* home_env = std::getenv("HOME");
        
        if (home_env != nullptr) {
            // Replace the '~' with the actual home directory path
            // path.substr(1) grabs everything AFTER the '~' (like "/Documents")
            path = std::string(home_env) + path.substr(1);
        } else {
            std::cerr << "cd: HOME not set\n";
            return;
        }
    }

  if(chdir(path.c_str()) != 0) {
    std::cout << "cd: " << path << ": No Such file or directory\n";
  }
}

// function to execute external programs
void executeExternal(const std::vector<std::string>& tokens) {
  std::vector<char*> c_args;

  for(size_t i=0; i<tokens.size(); i++) {
    c_args.push_back(const_cast<char*>(tokens[i].c_str()));
  }
  c_args.push_back(nullptr);

  pid_t pid = fork();
  if(pid == 0) {
    execvp(c_args[0], c_args.data()); // This will execute the command in child process
    std::cout << tokens[0] << ": command not found\n"; // if the process success then this line won't work unless failed
    exit(1);
  } else if(pid > 0) {  // pid_t waitpid(pid_t pid, int *status, int options);
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
    std::cout << "\n$ ";

    if(!std::getline(std::cin, input)) break;
    if(input.empty()) continue; // Ignore empty Enter presses

    std::vector<std::string> tokens = parseInput(input);
    if(tokens.empty()) continue;

    std::string cmd = tokens[0];

    if(cmd == "exit"){
      break;
    } else if (cmd == "echo") {
      executeEcho(tokens);
    } else if (cmd == "type") {
      if(tokens.size() > 1) checkType(tokens[1]);
    } else if (cmd == "pwd") {
      executePwd();
    } else if (cmd == "cd") {
      if(tokens.size() > 1) executeCd(tokens[1]);
    } else {
      executeExternal(tokens);
    }
  }


  return 0;
}