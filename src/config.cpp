#include "shell.h"
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

std::string getConfigFilePath() {
    const char* home_env = std::getenv("HOME");
    if(home_env != nullptr) {
        return std::string(home_env) + "/.myshell_config";
    }
    return "";
}

void loadConfig() {
    std::string path = getConfigFilePath();
    if (path.empty()) return;

    std::ifstream infile(path);
    if (!infile.is_open()) return;

    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty() || line[0] == '#') continue; // # to avoid the line

        size_t delim = line.find('=');
        if (delim != std::string::npos) {
            std::string key = line.substr(0, delim);
            std::string value = line.substr(delim + 1);

            if (key == "prompt_color") shell_config.prompt_color = value;
            else if (key == "show_username") shell_config.show_username = (value == "true" || value == "1");
            else if (key == "enable_suggestions") shell_config.enable_suggestions = (value == "true" || value == "1");
        }
    }
    infile.close();
}

void saveConfig() {
    std::string path = getConfigFilePath();
    if (path.empty()) return;

    std::ofstream outfile(path); // output file stead to save the text in file
    if (outfile.is_open()) {
        outfile << "prompt_color=" << shell_config.prompt_color << "\n";
        outfile << "show_username=" << (shell_config.show_username ? "true" : "false") << "\n";
        outfile << "enable_suggestions=" << (shell_config.enable_suggestions ? "true" : "false") << "\n";
        outfile.close();
    }
}

void showConfigUI() {
    std::cout << "\033[?1049h\033[?25l"; // ANSI escape code, ?1049h = turn on alternate screen buffer, ?251 = turn off cursor
    
    // switch canonical mode to raw mode
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // tract the state of UI
    int selected = 0;
    const int NUM_OPTIONS = 4;
    const int MAX_WIDTH = 50;

    std::vector<std::pair<std::string, std::string>> colors = {
        {"Green", "\033[1;32m"}, {"Blue", "\033[1;34m"}, {"Cyan", "\033[1;36m"},
        {"Red", "\033[1;31m"}, {"Magenta", "\033[1;35m"}, {"Yellow", "\033[1;33m"}
    };

    // lambda function get the color name
    auto getColorIndex = [&](const std::string& code) {
        for (size_t i = 0; i < colors.size(); i++) if (colors[i].second == code) return i;
        return (size_t)0;
    };

    // get the picked color code by name
    int p_color_idx = getColorIndex(shell_config.prompt_color);
    bool running = true;

    while (running) {
        struct winsize w; // window size structure which holds the details of screen structure
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // input/output control, Terminal I/O Window Size
        int cols = w.ws_col; // get the column of 
        
        int menu_width = (cols < MAX_WIDTH) ? cols : MAX_WIDTH; // calculate the width if going out of max width to keep still
        int left_pad_amount = (cols - menu_width) / 2; // make it center, just calculate the left space to make it
        std::string pad(left_pad_amount > 0 ? left_pad_amount : 0, ' '); // add space before insert the text to make it feel center

        std::cout << "\033[2J\033[H"; // 2J = clear entire screen,H = moves the invisible drawing pen back to top-left corner, to make old text gets wiped away at every frame starts

        // Centered Header
        std::string title = "=== MyShell Configuration ===";
        int title_pad = (menu_width - title.length()) / 2;
        std::cout << "\n\n" << pad << std::string(title_pad > 0 ? title_pad : 0, ' ') << "\033[1;36m" << title << "\033[0m\n\n\n";

        // Draw Options
        for (int i = 0; i < NUM_OPTIONS; i++) {
            std::cout << pad; 

            if (i == NUM_OPTIONS-1) {
                // The "Save & Exit" Button
                std::string btn = "[ Save & Exit ]";
                int btn_pad = (menu_width - btn.length()) / 2;
                
                // If selected, apply Green + Invert to make a solid green block!
                if (i == selected) std::cout << "\033[1;32m\033[7m";  // 1;32m makes bold green, 7m is invert command to change color of whatever
                else std::cout << "\033[1;32m"; // Just green text if not selected
                
                std::cout << std::string(btn_pad, ' ') << btn << std::string(menu_width - btn_pad - btn.length(), ' ');
                std::cout << "\033[0m"; 
            } else {
                if (i == selected) std::cout << "\033[7m"; 

                std::string left_text = (i == selected ? "> " : "  ");
                std::string right_text = "";
                std::string right_color = "";

                if (i == 0) {
                    left_text += "Prompt Color";
                    right_text = colors[p_color_idx].first;
                    right_color = colors[p_color_idx].second;
                } else if (i == 1) {
                    left_text += "Show Username";
                    right_text = shell_config.show_username ? "[ON]" : "[OFF]";
                } else if (i == 2) {
                    left_text += "Enable Suggestions";
                    right_text = shell_config.enable_suggestions ? "[ON]" : "[OFF]";
                }

                int spaces_needed = menu_width - left_text.length() - right_text.length();
                std::string middle_spaces(spaces_needed > 0 ? spaces_needed : 1, ' ');

                std::cout << left_text << middle_spaces << right_color << right_text << "\033[0m";
            }

            // The gap between stack options
            std::cout << "\n\n"; 
        }

        // The Info Footer with an extra gap above it
        std::string footer = "UP/DOWN: Select   LEFT/RIGHT: Change   Q: Quit";
        int footer_pad = (menu_width - footer.length()) / 2;
        // \033[90m is the ANSI code for faded grey text
        std::cout << "\n" << pad << std::string(footer_pad > 0 ? footer_pad : 0, ' ') << "\033[90m" << footer << "\033[0m\n";

        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == '\033') {
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                    if (seq[0] == '[') {
                        if (seq[1] == 'A') selected = (selected > 0) ? selected - 1 : NUM_OPTIONS - 1; // UP
                        if (seq[1] == 'B') selected = (selected < NUM_OPTIONS - 1) ? selected + 1 : 0; // DOWN
                        if (seq[1] == 'C') { // RIGHT
                            if (selected == 0) p_color_idx = (p_color_idx + 1) % colors.size();
                            if (selected == 1) shell_config.show_username = !shell_config.show_username;
                            if (selected == 2) shell_config.enable_suggestions = !shell_config.enable_suggestions;
                        }
                        if (seq[1] == 'D') { // LEFT
                            if (selected == 0) p_color_idx = (p_color_idx == 0) ? colors.size() - 1 : p_color_idx - 1;
                            if (selected == 1) shell_config.show_username = !shell_config.show_username;
                            if (selected == 2) shell_config.enable_suggestions = !shell_config.enable_suggestions;
                        }
                    }
                }
            } else if (c == '\n') {
                if (selected == 3) {
                    shell_config.prompt_color = colors[p_color_idx].second;
                    saveConfig(); 
                    running = false;
                } else if (selected == 1) shell_config.show_username = !shell_config.show_username;
                else if (selected == 2) shell_config.enable_suggestions = !shell_config.enable_suggestions;
            } else if (c == 'q') {
                running = false; 
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // connect to old terminal settings
    std::cout << "\033[?1049l\033[?25h"; // ?10491 = turn off alternate buffer and back to normal terminal, ?25h = turn on cursor
}