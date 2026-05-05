#include "shell.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>

int getNextAvailableJobId() {
  int id = 1;
  while (true) {
    bool in_use = false;
    for(size_t i=0; i<background_jobs.size(); i++) {
      if(background_jobs[i].job_id == id) {
        in_use = true;
        break;
      }
    }

    if(!in_use) {
      return id;
    }
    id++;
  }
}

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

      int assigned_id = getNextAvailableJobId();
      Job new_job = {assigned_id, pid, cmd_str, true}; // create the struct
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