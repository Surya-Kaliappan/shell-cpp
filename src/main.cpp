#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <sstream> // for string stream
#include <unistd.h> // for access() and X_OK and getcwd()
#include <sys/wait.h> // for waitpid()
#include <fcntl.h> // for open(), O_WRONLY, O_CREAT, O_TRUNC
#include <termios.h> // for raw mode terminal control
#include <dirent.h> // for opening and reading directories
#include <set> // to store unique autocomplete matches

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
    } else if(c == '\\' && in_double) {  // Backslash in double quotes inside
      if(i+1 < input.length()) {
        char next_c = input[i+1];
        if(next_c == '"' || next_c == '\\' || next_c == '$' || next_c == '`' || next_c == '\n') {
          current += next_c;
          i++;
        } else {
          current += c;
        }
        in_word = true;
      } else {
        current += c;
        in_word = true;
      }
    } else if(c == '\'' && !in_double) {
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
    std::cerr << tokens[0] << ": command not found\n"; // if the process success then this line won't work unless failed
    exit(1);
  } else if(pid > 0) {  // pid_t waitpid(pid_t pid, int *status, int options);
    int status;  // this vaiable will hold the exit status of the child process
    waitpid(pid, &status, 0); // wait for the child process to finish
  } else {
    std::cerr << "Fork failed\n";
  }
}

bool executeCommand(std::vector<std::string>& tokens) {
  if(tokens.empty()) return true;

  // Find and Extract Redirection
  std::string output_file = "";
  int target_fd = -1; // this hold the pipe to redirect
  bool append_mode = false; // Track if we are appending or overwriting

  for(size_t i=0; i<tokens.size(); i++) {
    if(tokens[i] == ">>" || tokens[i] == "1>>") {
      if(i+1 < tokens.size()) {
        output_file = tokens[i+1];
        target_fd = STDOUT_FILENO;
        append_mode = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        break;
      }
    } else if(tokens[i] == ">" || tokens[i] == "1>") {
      if(i+1 < tokens.size()) {
        output_file = tokens[i+1];  // store the file name
        target_fd = STDOUT_FILENO; // Pipe 1
        append_mode = false;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2); // erase the  > and filename from the tokens
        break;
      }
    } else if(tokens[i] == "2>>") {
      if(i+1 < tokens.size()) {
        output_file = tokens[i+1];
        target_fd = STDERR_FILENO;
        append_mode = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        break;
      }
    } else if(tokens[i] == "2>") {
      if(i+1 < tokens.size()) {
        output_file = tokens[i+1];
        target_fd = STDERR_FILENO;  // Pipe 2
        append_mode = false;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        break;
      }
    }
  }

  if(tokens.empty()) return true;
  std::string cmd = tokens[0];

  // Handle Exit Early
  if(cmd == "exit") {
    return false;
  }

  // setup redirection
  int original_fd = -1;  // backup the original terminal output
  if(!output_file.empty() && target_fd != -1) {

    // flag decision to open the file
    int flags = O_WRONLY | O_CREAT;  // turn mode to write, create if not exist
    if(append_mode) {
      flags |= O_APPEND;  // append flag
    } else {
      flags |= O_TRUNC;  // truncate (overwrite) flag
    }
    
    int fd = open(output_file.c_str(), flags, 0644); //  0644 for file permission
    if(fd != -1) {
      original_fd = dup(target_fd);  // backup the File Descriptor 4 which give 1 as value.
      dup2(fd, target_fd);  // this violently unplugs screen(File Descriptor 1) and plugs newly opened text file  
      close(fd);  // don't need the original file wire anymore because it is securely plugged into the STDOUT slot.
    } else {
      std::cerr << "Error opening file\n";
      return true;
    }
  }

  // Routing table
  if(cmd == "echo") {
    executeEcho(tokens);
  } else if(cmd == "type") {
    if(tokens.size() > 1) checkType(tokens[1]);
  } else if(cmd == "pwd") {
    executePwd();
  } else if(cmd == "cd") {
    if(tokens.size() > 1) executeCd(tokens[1]);
  } else {
    executeExternal(tokens);
  }

  // restore redirection
  if(original_fd != -1) {
    // flush() forces C++ to push every last letter out into the text file before we switch the wires back.
    if(target_fd == STDOUT_FILENO) std::cout.flush();
    if(target_fd == STDERR_FILENO) std::cerr.flush();
    dup2(original_fd, target_fd);  // take our backup wire (slot) and plug it back into the stored slot.
    close(original_fd);
  }

  return true;
}

std::vector<std::string> getCompletions(const std::string& prefix) {
  if(prefix.empty()) return {};

  std::set<std::string> matches;

  std::vector<std::string> builtins = {"echo", "exit", "type", "pwd", "cd"};
  for(size_t i=0; i<builtins.size(); i++) {
    if(builtins[i].rfind(prefix, 0) == 0) {
      matches.insert(builtins[i]);
    }
  }

  const char* path_env = std::getenv("PATH");
  if(path_env != nullptr) {
    std::stringstream ss(path_env);
    std::string dir;

    while(std::getline(ss, dir, ':')) {  // separate the path string by : and used to loop
      DIR* dp = opendir(dir.c_str());  // POSIX system call to open the folder. it returns "Directory Pointer"(dp).
      if(dp != nullptr) {
        struct dirent* entry;  // Directory Entry: This is struct that holds the information for a single file inside the folder.

        while((entry = readdir(dp)) != nullptr) { // Every call, the OS move the cursor to next file in the folder, it also make the loop working until nullptr.
          std::string name = entry->d_name;  // grabs the actual name of the file.

          // Every single folder in UNIX contains a hidden link to itself and parent, So skip these.
          if(name == "." || name == "..") continue; // '.' -> current, '..' -> parent

          if(name.rfind(prefix, 0) == 0) {
            std::string full_path = dir + "/" + name;
            if(access(full_path.c_str(), X_OK) == 0) {
              matches.insert(name);
            }
          }
        }
        closedir(dp); // must to close the opened directory to avoid memory leak
      }
    }
  }

  return std::vector<std::string>(matches.begin(), matches.end());
}

bool readLine(std::string& input) {
  termios oldt, newt;  // termios is a struct that holds the settings for terminal(colors, speed, modes)
  tcgetattr(STDIN_FILENO, &oldt);  // tcgetattr gets the current attributes of terminal
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);  // disable buffering and auto-printing
  tcsetattr(STDIN_FILENO, TCSANOW, &newt); // TCSANOW means tells the OS to apply these changes "NOW"

  input.clear();
  char c;
  int tab_count = 0;  // handle tab count

  while(true) {
    int n = read(STDIN_FILENO, &c, 1);
    if(n <= 0 || c == 4) {  // If EOF or Ctrl-D is pressed
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      return false;  // main loop to break
    }

    if(c == '\n') {
      write(STDOUT_FILENO, "\n", 1); // print the enter key
      break;
    } else if(c == 127) { // Backspace Key
      if(!input.empty()) {
        input.pop_back();
        // Move cursor back, print a space to erase, move cursor back again
        write(STDOUT_FILENO, "\b \b", 3);
      }
    } else if(c == '\t') {  // TAB key (Autocomplete)
      tab_count++;
      std::vector<std::string> matches = getCompletions(input);

      // if found exactly one match, autocomplete it!
      if(matches.size() == 1) {
        // Grab the missing letters
        std::string completion = matches[0].substr(input.length()) + " ";
        input += completion;
        // Print the missing letters to the screen
        write(STDOUT_FILENO, completion.c_str(), completion.length());
      } else if(matches.size() > 1) {
        if(tab_count == 1) {
          write(STDOUT_FILENO, "\a", 1);
        } else if(tab_count >= 2) {
          write(STDOUT_FILENO, "\n", 1);

          for(size_t i=0; i<matches.size(); i++) {
            write(STDOUT_FILENO, matches[i].c_str(), matches[i].length());
            if(i != matches.size() - 1) {
              write(STDOUT_FILENO, "  ", 2);
            }
          }
          write(STDOUT_FILENO, "\n", 1);
          write(STDOUT_FILENO, "$ ", 2);
          write(STDOUT_FILENO, input.c_str(), input.length());

          tab_count = 0;
        }
      } else {
        write(STDOUT_FILENO, "\a", 1);
        tab_count = 0;
      }
    } else {
      tab_count = 0;
      // Normal characters
      input += c;
      write(STDOUT_FILENO, &c, 1); // Manually print the letter just types
    }
  }

  // Restore normal Cooked Mode before running the command
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return true;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string input;

  while(true) {
    std::cout << "\n$ ";
    std::cout.flush(); // flush the prompt before turning on Raw Mode

    if(!readLine(input)) break;  // exit if Ctrl-D is pressed
    if(input.empty()) continue;

    std::vector<std::string> tokens = parseInput(input);

    if(!executeCommand(tokens)) {
      break;
    }
  }

  return 0;
}