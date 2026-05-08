#ifndef SHELL_H
#define SHELL_H

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unistd.h>

// structs
struct Job {
    int job_id;
    pid_t pid;
    std::string command;
    bool is_running;
};

struct Config {
    std::string prompt_color = "\033[1;32m"; // default green
    std::string prompt_symbol = "$";
    bool enable_suggestions = true;
};

// Global Variables
extern std::vector<std::string> command_history;
extern size_t history_sync_index;
extern std::vector<Job> background_jobs;
extern std::unordered_map<std::string, std::string> completion_scripts;
extern const std::vector<std::string> BUILTINS;
extern Config shell_config;

// Functions
// Parser
std::vector<std::string> parseInput(const std::string& input);

// Execution
bool executeCommand(std::vector<std::string>& tokens);
bool executePipeline(const std::vector<std::string>& tokens);
void executeExternal(const std::vector<std::string>& tokens, bool is_background);
void runWorker(const std::vector<std::string>& tokens);

// Builtins
bool runBuiltin(const std::vector<std::string>& tokens);
void checkType(const std::string& args);
void executeEcho(const std::vector<std::string>& tokens);
void executePwd();
void executeCd(std::string path);
void executeHistory(const std::vector<std::string>& tokens);
void executeJobs(bool show_all);
void executeComplete(const std::vector<std::string>& tokens);

// Autocomplete
std::vector<std::string> getCompletions(const std::string& prefix);
std::string getLongestCommonPrefix(const std::vector<std::string>& matches);
std::vector<std::string> getProgrammableCompletions(const std::string& script_path, const std::string& base_cmd, const std::string& search_term, const std::string& previous_word, const std::string& full_input);
std::vector<std::string> getFileCompletions(const std::string& search_term);

// UI / IO
bool readLine(std::string& input);
int getNextAvailableJobId();

// Config Engine
void loadConfig();
void saveConfig();
void showConfigUI();
std::string buildPrompt();

#endif // SHELL_H