#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream> // for string stream
#include <unistd.h> // for access() and X_OK and getcwd() and pipe()
#include <sys/wait.h> // for waitpid()
#include <fcntl.h> // for open(), O_WRONLY, O_CREAT, O_TRUNC
#include <termios.h> // for raw mode terminal control
#include <dirent.h> // for opening and reading directories
#include <set> // to store unique autocomplete matches
#include <iomanip> // for std::setw()
#include <fstream> // for std::ifstream
#include <sys/stat.h> // for chmod()

// Global Constants
struct Job {
  int job_id;
  pid_t pid;
  std::string command;
  bool is_running;
};

const std::vector<std::string> BUILTINS = {"echo", "exit", "type", "pwd", "cd", "history", "jobs"};
std::vector<std::string> command_history;
size_t history_sync_index = 0;
std::vector<Job> background_jobs;
int next_job_id = 1;

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

// BUILTINS functions
// function to find the either type or executable file path
void checkType(const std::string& args) {
  if(args.empty()) return ;

  for(size_t i=0; i<BUILTINS.size(); i++) {
    if(args == BUILTINS[i]) {
      std::cout << args << " is a shell builtin\n";
      return;
    }
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

void executeHistory(const std::vector<std::string>& tokens) {

  if(tokens.size() >= 3) {
    std::string flag = tokens[1];
    std::string file_path = tokens[2];

    if(flag == "-r") { // read from file
      std::ifstream infile(file_path); // Input File Stream: This gets the file from hard drive if permission to read
    
      if(infile.is_open()) { // it prevents program to crash if data pull without connect to file
        std::string line;
        while(std::getline(infile, line)) { // This pulls the characters from file
          command_history.push_back(line);
        }
        infile.close(); // close the connection
      } else {
        std::cerr << "history: " << file_path << ": cannot open file\n";
      }
    } else if (flag == "-w") { // write to file
      std::ofstream outfile(file_path); // Output File Stream: this creates file in hard drive and write if permission 
    
      if(outfile.is_open()) {
        chmod(file_path.c_str(), 0600); // to make the file only read 'n write by owner
        for(size_t i=0; i<command_history.size(); i++) {
          outfile << command_history[i] << "\n";  // outfile would handle the recieved data to write in file
        }
        outfile.close(); // close the connection
        history_sync_index = command_history.size(); // update the sync value
      } else {
        std::cerr << "history: " << file_path << ": cannot create file\n";
      }
    } else if(flag == "-a") { // append to file
      std::ofstream outfile(file_path, std::ios::app); // ios::app would tell the stream to change append mode
    
      if(outfile.is_open()) {
        chmod(file_path.c_str(), 0600);
        for(size_t i=history_sync_index; i<command_history.size(); i++) {
          outfile << command_history[i] << "\n";
        }
        outfile.close();
        history_sync_index = command_history.size(); // update the sync value
      } else {
        std::cerr << "history: " << file_path << ": cannot open file\n";
      }
    }
    return;
  }

  size_t start_index = 0;

  // checking if there is <n> argument
  if(tokens.size() > 1) {
    try {
      size_t n = std::stoi(tokens[1]); // convert the string to an integer
      if(n<=0) return;

      if(static_cast<size_t>(n) < command_history.size()) {
        start_index = command_history.size() - n;
      }
    } catch (...) {
      // std::stoi might fail due to giving no numeric values
      start_index = 0;
    }
  }
  for(size_t i=start_index; i<command_history.size(); i++) {
    std::cout << std::setw(5) << (i+1) << " " << command_history[i] << "\n";
  }
}

void executeJobs(bool show_all) {
  if(background_jobs.empty()) return;

  std::vector<Job> active_jobs; // update the active jobs to confirm at final
  size_t n = background_jobs.size();

  for(size_t i=0; i<n; i++) {
    Job& job = background_jobs[i]; // take the job reference instead of copy, cause it affect the original if changes happen

    if(job.is_running) {
      int status;
      if(waitpid(job.pid, &status, WNOHANG) > 0) { // check child status and move on, if working it return 0
        job.is_running = false; // this changes the status of child as finished by use of pointer directly
      }
    }

    if(show_all || !job.is_running) { // if jobs hit show_all will true, if false then it should show the done process for that avoid the running process
      char marker = (i == n-1) ? '+' : (i == n-2) ? '-' : ' ';
      std::string state = job.is_running ? "Running" : "Done";
      std::string ampersand = job.is_running ? " &" : "";

      std::cout << "[" << job.job_id << "]" << marker << "  "
                << std::left << std::setw(24) << state
                << job.command << ampersand << "\n";
    }

    if(job.is_running) {
      active_jobs.push_back(job); // if process running then update in active job.
    }
  }

  background_jobs = active_jobs; // At final assign the active job to background jobs, just like update the background_jobs but not all value just pointer.
}

// EXTERNAL functions
// function to execute external programs
void executeExternal(const std::vector<std::string>& tokens, bool is_background = false) {
  std::vector<char*> c_args;  // char* is used to store string like in C.

  for(size_t i=0; i<tokens.size(); i++) {
    c_args.push_back(const_cast<char*>(tokens[i].c_str()));
  }
  c_args.push_back(nullptr);

  pid_t pid = fork();  // This creates clone of this whole execution and run separately
  if(pid == 0) {  // fork return two pid for parent and chile, and both of them have this same if conditions.
    execvp(c_args[0], c_args.data()); // This will execute the command in child process
    std::cerr << tokens[0] << ": command not found\n"; // if the process success then this line won't work unless failed
    exit(1);
  } else if(pid > 0) {
    if(is_background) { // just ask to don't wait for child to complete
      std::string cmd_str = "";
      for(size_t i=0; i<tokens.size(); i++) {
        cmd_str += tokens[i] + (i == tokens.size() - 1 ? "" : " ");
      }

      Job new_job = {next_job_id++, pid, cmd_str, true}; // create the struct
      background_jobs.push_back(new_job); // store the struct

      std::cout << "[" << new_job.job_id << "]" << new_job.pid << "\n";
    } else { // ask to wait for child to complete
      int status;  // this vaiable will hold the exit status of the child process
      // pid_t waitpid(pid_t pid, int *status, int options);
      waitpid(pid, &status, 0); // wait for the child process to finish
    }
  } else {
    std::cerr << "Fork failed\n";
  }
}

// Built-in Router
bool runBuiltin(const std::vector<std::string>& tokens) {
  if(tokens.empty()) return false;
  std::string cmd = tokens[0];

  if(cmd == "echo") { executeEcho(tokens); return true; }
  if(cmd == "history") { executeHistory(tokens); return true; }
  if(cmd == "jobs") { executeJobs(true); return true; }
  if(cmd == "type") { if(tokens.size() > 1) checkType(tokens[1]); return true; }
  if(cmd == "pwd") { executePwd(); return true; }
  if(cmd == "cd") { if(tokens.size() > 1) executeCd(tokens[1]); return true; }
  if(cmd == "exit") { return true; }

  return false;
}

// helper function for execvp from child
void runWorker(const std::vector<std::string>& tokens) {
  if(runBuiltin(tokens)) exit(0); // To run the Builtin commands

  std::vector<char*> c_args;
  for(size_t i=0; i<tokens.size(); i++) {
    c_args.push_back(const_cast<char*>(tokens[i].c_str()));
  }
  c_args.push_back(nullptr);

  execvp(c_args[0], c_args.data());
  std::cerr << tokens[0] << ": command not found\n";
  exit(1);
}

// function to execute the pipeline commands
bool executePipeline(const std::vector<std::string>& tokens) {
	std::vector<std::vector<std::string>> commands; // commands store in dynamic array method
  std::vector<std::string> current_cmd; // used for temporary command store

  for(size_t i=0; i<tokens.size(); i++) {
    if(tokens[i] == "|") {
      if(!current_cmd.empty()) {
        commands.push_back(current_cmd);
        current_cmd.clear();
      }
    } else {
      current_cmd.push_back(tokens[i]);
    }
  }
  if(!current_cmd.empty()) commands.push_back(current_cmd); // for getting the last command after the loop

  int num_cmds = commands.size(); // number of commands
  std::vector<pid_t> pids; // creating vector to store pids

  int prev_read_fd = -1; // stores the reading pipe of previous one

  for(int i=0; i<num_cmds; i++) {
    int pipefd[2];

    // creating pipe for every command except last one because of only print normal screen
    if(i < num_cmds-1) {
      if(pipe(pipefd) == -1) {  // pipefd[0](out) for reading and pipefd[1](in) for writing
        std::cerr << "pipe failed\n";
        return true;
      }
    }

    // creating clone which returns two pids, one for child(pid 0) and another parent
    pid_t pid = fork();
    if(pid == 0) {

      // every command should get the data from previous pipe except first one
      if(i > 0) {
        dup2(prev_read_fd, STDIN_FILENO); // change the input direction from previous read pipe
        close(prev_read_fd);
      }

      // every command should put the data to next pipe except last one
      if(i < num_cmds-1) {
        dup2(pipefd[1], STDOUT_FILENO); // change the output direction for next pipe
        close(pipefd[0]);
        close(pipefd[1]);
      }

      runWorker(commands[i]);  // pass the command which handles builtins and rest things
    }

    pids.push_back(pid); // keep tracking the clones by storing
    if(prev_read_fd != -1) {
      close(prev_read_fd); // closing the previous reading pipe
    }

    if(i < num_cmds-1) {
      prev_read_fd = pipefd[0];  // assigning the read pipe to next clone
      close(pipefd[1]); // close the write pipe
    }
  }

  // sleep until the clone in list to finish and die
  for(size_t i=0; i<pids.size(); i++) {
    int status;
    waitpid(pids[i], &status, 0);
  }

  return true;
}

bool executeCommand(std::vector<std::string>& tokens) {
  if(tokens.empty()) return true;

  // check the background job assign
  bool is_background = false;
  if(tokens.back() == "&") {
    is_background = true;
    tokens.pop_back(); // remove the last token "&"
  }

  if(tokens.empty()) return true;

  // separately check the pipe occurance in the command
  for(size_t i=0; i<tokens.size(); i++) {
    if(tokens[i] == "|") {
      return executePipeline(tokens);
    }
  }
 
  // Find and Extract Redirection
  std::string output_file = "";
  int target_fd = -1; // this hold the pipe to redirect
  bool append_mode = false; // Track if we are appending or overwriting

  for(size_t i = 0; i < tokens.size(); i++) {
    bool is_redirect = true;

    if (tokens[i] == ">" || tokens[i] == "1>") {
      target_fd = STDOUT_FILENO;
      append_mode = false;
    } else if (tokens[i] == ">>" || tokens[i] == "1>>") {
      target_fd = STDOUT_FILENO;
      append_mode = true;
    } else if (tokens[i] == "2>") {
      target_fd = STDERR_FILENO;
      append_mode = false;
    } else if (tokens[i] == "2>>") {
      target_fd = STDERR_FILENO;
      append_mode = true;
    } else {
      is_redirect = false; 
    }

    // If we found a redirect operator, extract the file and erase the tokens ONCE
    if (is_redirect && i + 1 < tokens.size()) {
      output_file = tokens[i + 1];
      tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
      break; 
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
  if(!runBuiltin(tokens)){
    executeExternal(tokens, is_background);
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

  for(size_t i=0; i< BUILTINS.size(); i++) {
    if(BUILTINS[i].rfind(prefix, 0) == 0) {
      matches.insert(BUILTINS[i]);
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

std::string getLongestCommonPrefix(const std::vector<std::string>& matches) {
  if(matches.empty()) return "";

  std::string prefix = matches[0];  // assuming first element as prefix

  for(size_t i=1; i<matches.size(); i++) {
    size_t j=0;
    while(j < prefix.length() && j < matches[i].length() && prefix[j] == matches[i][j]) {
      j++;
    }
    prefix = prefix.substr(0,j); // starts from ith index to add j characters.
    if(prefix.empty()) break; // emergency stop if no prefix at all
  }

  return prefix;
}

std::vector<std::string> getFileCompletions(const std::string& search_term) {
  std::set<std::string> matches;

  std::string dir_path = "."; // Default current Directory
  std::string prefix = search_term; // Default to whole search term

  size_t last_slash = search_term.find_last_of('/');
  if(last_slash != std::string::npos) {
    dir_path = search_term.substr(0, last_slash + 1);
    prefix = search_term.substr(last_slash + 1);
  }

  DIR* dp = opendir(dir_path.c_str());  // get the working directory
  if(dp != nullptr) {
    struct dirent* entry; // structure for directory details which holds every files details

    while((entry = readdir(dp)) != nullptr) { // loop the files list
      std::string name = entry->d_name; // get the file name
      if(name == "." || name == "..") continue; // evade the current and previous directory sysmbol which default occur

      if(name.rfind(prefix, 0) == 0) {
        std::string full_path = (dir_path == "." ? name : dir_path + name);

        struct stat statbuf;
        bool is_dir = false;
        if(stat(full_path.c_str(), &statbuf) == 0) { // tell to go and find the file and fills out the folder, if exists return 0.
          if(S_ISDIR(statbuf.st_mode)) { // if stat() finish ok, then statbuf would have data. one of it variable struct is st_mode(permission + type details)
            is_dir = true;              // This S_ISDIR check the signature of the directory with st_mode which has messy combination
          }
        }

        std::string match_str = (last_slash != std::string::npos) ? (dir_path + name) : name;
        if(is_dir) {
          match_str += "/";
        } else {
          match_str += " ";
        }

        matches.insert(match_str);
      }
    }
    closedir(dp);
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

  // starts at very end of the history list
  int history_index = command_history.size();

  while(true) {
    int n = read(STDIN_FILENO, &c, 1);
    if(n <= 0 || c == 4) {  // If EOF or Ctrl-D is pressed
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      return false;  // main loop to break
    }

    // arrow keys
    if(c == '\033') { // up or down arrow gives linux OS as ^[[A (esc + [ + A or B)
      char seq[2]; // once got escape character then store other two character in sequence
      if(read(STDIN_FILENO, &seq[0], 1) <= 0) continue; // if fails to get the character then skip the loop.
      if(read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

      if(seq[0] == '[') {
        if(seq[1] == 'A') { // up arrow ^[[A
          if(history_index > 0) { // check upto last command
            history_index--;
            input = command_history[history_index];

            std::string redraw = "\033[2K\r$ " + input; // \033 -> escape character, [ -> CSI, 2 -> horizontal full line, K -> erasae signal, \r -> to reach cursor to starting point
            write(STDOUT_FILENO, redraw.c_str(), redraw.length());  // write the terminal
          }
        } else if(seq[1] == 'B') { // down arrow ^[[B
          if(history_index < (int)command_history.size()) { // history_index is integer, and size() would return size_t, it won't allow negative, when compare that might problem
            history_index++;

            if(history_index == (int)command_history.size()) { // reach the end of the history list to show the empty string
              input = "";
            } else {
              input = command_history[history_index];
            }

            std::string redraw = "\033[2K\r$ " + input;
            write(STDOUT_FILENO, redraw.c_str(), redraw.length());
          }
        }
      }
      tab_count = 0;
      continue;
    } else if(c == '\n') {
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
      std::string search_term = input;
      std::vector<std::string> matches;

      size_t last_space = input.find_last_of(' ');
      if(last_space != std::string::npos) { // if no space was there the it fails and do else
        search_term = input.substr(last_space + 1);
        matches = getFileCompletions(search_term);
      } else {
        matches = getCompletions(search_term);
      }

      // if found exactly one match, autocomplete it!
      if(matches.size() == 1) {
        // Grab the missing letters
        std::string completion = matches[0].substr(search_term.length());

        input += completion;
        // Print the missing letters to the screen
        write(STDOUT_FILENO, completion.c_str(), completion.length());
        tab_count = 0;
      } else if(matches.size() > 1) {
        std::string lcp = getLongestCommonPrefix(matches);

        if(lcp.length() > search_term.length()) {
          std::string added_chars = lcp.substr(search_term.length());  // starts from ith index to end.
          input += added_chars;
          write(STDOUT_FILENO, added_chars.c_str(), added_chars.length());

        } else {
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
            write(STDOUT_FILENO, "\n$ ", 3);
            write(STDOUT_FILENO, input.c_str(), input.length());

            tab_count = 0;
          }
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

  // load history 
  std::string histfile_path = "";
  const char* home_env = std::getenv("HOME"); // get the root file path

  if(home_env != nullptr) {
    histfile_path = std::string(home_env) + "/.myshell_history";  // add the history file path
    setenv("HISTFILE", histfile_path.c_str(), 1); // set the HISTFILE value in env
  } else {
    std::cerr << "Warning: HOME not set. History will not be saved\n";
  }

  if(!histfile_path.empty()) {
    std::ifstream infile(histfile_path);  // read the history file
    if(infile.is_open()) { // if exsists open
      std::string line;
      while (std::getline(infile, line)) {
        command_history.push_back(line);
      }
      infile.close();

      history_sync_index = command_history.size();
    }
  }

  std::string input;

  while(true) {
    executeJobs(false); // check job details before every command executes
    std::cout << "\n$ ";

    if(!readLine(input)) break;  // exit if Ctrl-D is pressed
    if(input.empty()) continue;

    command_history.push_back(input);

    std::vector<std::string> tokens = parseInput(input);

    if(!executeCommand(tokens)) {
      break;
    }
  }

  if(!histfile_path.empty()) {
    std::ofstream outfile(histfile_path, std::ios::app); // apply append mode

    if(outfile.is_open()) {
      chmod(histfile_path.c_str(), 0600); // set the read n' write mode only for owner for security
      for(size_t i=history_sync_index; i<command_history.size(); i++) { // append remaining commands to file
        outfile << command_history[i] << "\n";
      }
      outfile.close(); // close the opened file
    }
  }

  return 0;
}
