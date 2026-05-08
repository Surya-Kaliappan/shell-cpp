// Internal Commands

#include "shell.h"
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iomanip>
// #include <cstdlib>

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

void executeComplete(const std::vector<std::string>& tokens) {
  if(tokens.size() < 3) return;

  std::string flag = tokens[1];

  if(flag == "-C" && tokens.size() >= 4) {
    std::string script_path = tokens[2];
    std::string target_cmd = tokens[3];

    completion_scripts[target_cmd] = script_path;
  } else if(flag == "-p") {
    std::string target_cmd = tokens[2];

    if(completion_scripts.find(target_cmd) != completion_scripts.end()) {
      std::cout << "complete -C '" << completion_scripts[target_cmd] << "' " << target_cmd << "\n";
    } else {
      std::cout << "complete: " << target_cmd << ": no completion specification\n";
    }
  } else if(flag == "-r") {
    std::string target_cmd = tokens[2];

    completion_scripts.erase(target_cmd);
  }
}

// Built-in Router
bool runBuiltin(const std::vector<std::string>& tokens) {
  if(tokens.empty()) return false;
  std::string cmd = tokens[0];

  if(cmd == "echo") { executeEcho(tokens); return true; }
  if(cmd == "history") { executeHistory(tokens); return true; }
  if(cmd == "jobs") { executeJobs(true); return true; }
  if(cmd == "complete") { executeComplete(tokens); return true; }
  if(cmd == "type") { if(tokens.size() > 1) checkType(tokens[1]); return true; }
  if(cmd == "pwd") { executePwd(); return true; }
  if(cmd == "cd") { if(tokens.size() > 1) executeCd(tokens[1]); return true; }
  if(cmd == "exit") { return true; }

  return false;
}