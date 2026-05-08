// Enter point and Globals

#include "shell.h"
#include <fstream>
#include <sys/stat.h>
#include <cstdlib>

std::vector<std::string> command_history;
size_t history_sync_index = 0;
std::vector<Job> background_jobs;
std::unordered_map<std::string, std::string> completion_scripts;
const std::vector<std::string> BUILTINS = {"echo", "exit", "type", "pwd", "cd", "history", "jobs", "complete"};
Config shell_config;

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    loadConfig();

    if(argc > 1 && std::string(argv[1]) == "--config") {
        showConfigUI();
        return 0;
    }

    std::string histfile_path = "";
    const char* home_env = std::getenv("HOME");

    if(home_env != nullptr) {
        histfile_path = std::string(home_env) + "/.myshell_history";
        setenv("HISTFILE", histfile_path.c_str(), 1);
    } else {
        std::cerr << "Warning: HOME not set. History will not be saved\n";
    }

    if(!histfile_path.empty()) {
        std::ifstream infile(histfile_path);
        if(infile.is_open()) {
            std::string line;
            while(std::getline(infile, line)) {
                command_history.push_back(line);
            }
            infile.close();
            history_sync_index = command_history.size();
        }
    }

    std::string input;

    while(true) {
        executeJobs(false);

        std::cout << buildPrompt();
        if(!readLine(input)) break;
        if(input.empty()) continue;

        command_history.push_back(input);
        std::vector<std::string> tokens = parseInput(input);

        if(!executeCommand(tokens)) {
            break;
        }
    }

    if(!histfile_path.empty()) {
        std::ofstream outfile(histfile_path, std::ios::app);
        if(outfile.is_open()) {
            chmod(histfile_path.c_str(), 0600);
            for(size_t i=history_sync_index; i<command_history.size(); i++) {
                outfile << command_history[i] << "\n";
            }
            outfile.close();
        }
    }

    return 0;
}