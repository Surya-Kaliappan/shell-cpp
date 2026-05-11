// Completion Engine

#include "shell.h"
#include <dirent.h>
#include <sys/stat.h>
#include <set>
#include <algorithm>
#include <sstream>
// #include <cstdlib>

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

std::vector<std::string> getProgrammableCompletions(const std::string& script_path, const std::string& base_cmd, const std::string& search_term, const std::string& previous_word, const std::string& full_input) {
  std::vector<std::string> results;

  setenv("COMP_LINE", full_input.c_str(), 1);
  setenv("COMP_POINT", std::to_string(full_input.length()).c_str(), 1);

  std::string cmd = script_path + " " + base_cmd + " '" + search_term + "' '" + previous_word + "'";

  FILE* pipe = popen(cmd.c_str(), "r"); // automatically creates pipe, set wires, and execvp the string passed
  if(!pipe) {
    unsetenv("COMP_LINE");
    unsetenv("COMP_POINT");
    return results;
  }

  char buffer[1024];
  while(fgets(buffer, sizeof(buffer), pipe) != nullptr) { // This collect the result byte by byte from pipe to store in buffer
    std::string line(buffer);
    if(!line.empty() && line.back() == '\n') {
      line.pop_back();
    }
    if(!line.empty() && line.rfind(search_term, 0) == 0) {
      line += " ";
      results.push_back(line);
    }
  }

  pclose(pipe);
  unsetenv("COMP_LINE");
  unsetenv("COMP_POINT");

  std::sort(results.begin(), results.end());
  return results;  
}

std::vector<std::string> getFileCompletions(const std::string& search_term, const std::string& base_cmd) {
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
        bool is_executable = false;

        if(stat(full_path.c_str(), &statbuf) == 0) { // tell to go and find the file and fills out the folder, if exists return 0.
          if(S_ISDIR(statbuf.st_mode)) { // if stat() finish ok, then statbuf would have data. one of it variable struct is st_mode(permission + type details)
            is_dir = true;              // This S_ISDIR check the signature of the directory with st_mode which has messy combination
          }
        } 
        if(access(full_path.c_str(), X_OK) == 0) {
          is_executable = true;
        }

        if(base_cmd == "cd") {
          if(!is_dir) continue;
        } else if(base_cmd == search_term) {
          if(!is_dir && !is_executable) continue;
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