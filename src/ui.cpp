// Terminal Engine

#include "shell.h"
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <algorithm>

std::string getBottomPrompt() {
  std::string bottom = "\033[1;36m╰>\033[0m";  // ╰─
  std::string color = shell_config.prompt_color;
  std::string symbol = shell_config.prompt_symbol;

  if(geteuid() == 0) {
    color = "\033[1;31m";
    if(symbol == "$") symbol = "#";
  }

  return bottom + color + symbol + "\033[0m ";
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
  int cursor_pos = 0;
  std::string current_buffer = "";
  std::string current_ghost_text = "";

  // Menu state variable
  bool in_menu = false;
  std::vector<std::string> menu_options;
  int menu_selected = 0;
  std::string base_input_before_completion = "";

  // tracks scrolling window
  static int viewport_start_row = 0;

  // search state variable
  bool in_search = false;
  std::string search_query = "";
  int search_match_index = -1;
  bool was_in_search = false;

  // search handler
  auto updateSearch = [&]() {
    search_match_index = -1;
    if(search_query.empty()) return;
    for(int i=command_history.size()-1; i >= 0; i--) {
      if(command_history[i].find(search_query) != std::string::npos) {
        search_match_index = i;
        break;
      }
    }
  };


  auto redrawLine = [&]() {
    std::string out = "";
    if(was_in_search) {
      out += "\r\033[1A"; // move up one line to clear
    }

    out += "\r\033[0J"; // clear everything below
    was_in_search = in_search;

    if(in_search) {
      std::string match_str = (search_match_index >= 0 && search_match_index < (int)command_history.size())
                              ? command_history[search_match_index] : "";
      std::string highlighted_match = match_str;
      if(!search_query.empty() && !match_str.empty()) {
        size_t pos = match_str.find(search_query);
        if(pos != std::string::npos) {
          highlighted_match = "\033[37m" + match_str.substr(0, pos) + 
                              "\033[1;32m\033[4m" + search_query + "\033[0m\033[37m" + 
                              match_str.substr(pos + search_query.length()) + "\033[0m";
        }
      } else if(!match_str.empty()) {
        highlighted_match = "\033[37m" + match_str + "\033[0m";
      }
      out += getBottomPrompt() + highlighted_match + "\n";
      out += "\033[1;36mbck-i-search:\033[0m " + search_query + "_";
      write(STDOUT_FILENO, out.c_str(), out.length());
      return;
    }

    out += getBottomPrompt() + input;

    int menu_lines_drawn = 0;

    if(in_menu && !menu_options.empty()) {
      struct winsize w;
      ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
      int term_cols = w.ws_col > 0 ? w.ws_col : 80;
      int term_rows = w.ws_row > 0 ? w.ws_row : 24;

      auto getDisplayName = [](const std::string& str) {
        std::string clean = str;
        char last = clean.empty() ? ' ' : clean.back();
        if(last == '/' || last == ' ') clean.pop_back();
        size_t slash = clean.find_last_of('/');
        if(slash != std::string::npos) clean = clean.substr(slash + 1);
        return clean + last;
      };

      int max_width = 0;
      for(size_t i=0; i<menu_options.size(); i++) {
        max_width = std::max(max_width, (int)menu_options[i].length());
      }
      max_width += 2;

      int cols = std::max(1, term_cols / max_width);
      int total_rows = (menu_options.size() + cols - 1) / cols;

      int max_visible_rows = std::max(3, term_rows - 4);
      int selected_row = menu_selected / cols;

      // viewport scroll logic
      if(selected_row < viewport_start_row) {
        viewport_start_row = selected_row;
      } else if(selected_row >= viewport_start_row + max_visible_rows) {
        viewport_start_row = selected_row - max_visible_rows + 1;
      }

      out += "\n";
      menu_lines_drawn++;

      int end_row = std::min(total_rows, viewport_start_row + max_visible_rows);
      for(int r=viewport_start_row; r<end_row; r++) {
        for(int c=0; c<cols; c++) {
          int idx = r * cols + c;
          if(idx < (int)menu_options.size()) {
            std::string d_name = getDisplayName(menu_options[idx]);
            bool is_dir = (!menu_options[idx].empty() && menu_options[idx].back() == '/');

            if(idx == menu_selected) out += "\033[7m"; // highlight
            if(is_dir) out += "\033[1;34m";

            std::string item = " " + menu_options[idx] + " ";
            out += item;

            int padding = max_width - item.length();
            if(padding > 0) out += std::string(padding, ' ');

            if(idx == menu_selected) out += "\033[0m"; // reset highlight
          }
        }
        if(r < end_row - 1 || viewport_start_row + max_visible_rows < total_rows) {
          out += "\n";
          menu_lines_drawn++;
        }
      }

      if(viewport_start_row + max_visible_rows < total_rows) {
        out += "\033[90m... (scroll down for more) ...\033[0m";
      }
    }

    if(menu_lines_drawn > 0) {
      out += "\r\033[" + std::to_string(menu_lines_drawn) + "A";
    }

    current_ghost_text = "";
    if(shell_config.enable_suggestions && !input.empty() && !in_menu && cursor_pos == (int)input.length()) {
      for(int i=command_history.size()-1; i>=0; i--) {
        if(command_history[i].length() >= input.length() && command_history[i].rfind(input, 0) == 0) {
          current_ghost_text = command_history[i].substr(input.length());
          break;
        }
      }
    }

    out += "\r" + getBottomPrompt() + input;
    if(!current_ghost_text.empty()) {
      out += "\033[s"; // save the current position of cursor
      out += "\033[90m" + current_ghost_text + "\033[0m";
      out += "\033[u"; // restore cursor perfectly back to the saved position
    }

    if(cursor_pos < (int)input.length()) {
      out += "\033[" + std::to_string(input.length() - cursor_pos) + "D";
    }

    write(STDOUT_FILENO, out.c_str(), out.length());
  };

  auto closeMenu = [&]() {
    in_menu = false;
    menu_options.clear();
    viewport_start_row = 0;
    redrawLine();
  };

  while(true) {
    int n = read(STDIN_FILENO, &c, 1);
    if(n <= 0 || c == 4) {  // If EOF or Ctrl-D is pressed
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      return false;  // main loop to break
    }

    // Ctrl+R interceptor
    if(c == 18) {
      if(!in_search) {
        in_search = true;
        search_query = "";
        search_match_index = -1;
        if(in_menu) closeMenu();
      } else {
        if(search_match_index > 0) {
          for(int i=search_match_index-1; i>=0; i--) {
            if(command_history[i].find(search_query) != std::string::npos) {
              search_match_index = i;
              break;
            }
          }
        }
      }
      redrawLine();
      continue;
    }

    // input routing while in search mode
    if(in_search) {
      if(c == 127) {
        if(!search_query.empty()) {
          search_query.pop_back();
          updateSearch();
        }
        redrawLine();
        continue;
      } else if(c == '\n') {
        if(search_match_index >= 0) input = command_history[search_match_index];
        in_search = false;
        std::string final_out = "";
        if (was_in_search) final_out += "\r\033[1A";
        final_out += "\r\033[0J" + getBottomPrompt() + input + "\n";
        write(STDOUT_FILENO, final_out.c_str(), final_out.length());
        break;
      } else if(c == '\033' || c == '\t') {
        if(search_match_index >= 0) input = command_history[search_match_index];
        cursor_pos = input.length();
        in_search = false;
        redrawLine();
        if(c == '\t') continue;
      } else {
        search_query += c;
        updateSearch();
        redrawLine();
        continue;
      }
    }

    // arrow keys
    if(c == '\033') { // up or down arrow gives linux OS as ^[[A (esc + [ + A or B)
      char seq[2]; // once got escape character then store other two character in sequence
      if(read(STDIN_FILENO, &seq[0], 1) <= 0) continue; // if fails to get the character then skip the loop.
      if(read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

      if(seq[0] == '[') {

        if(in_menu) {
          struct winsize w;
          ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
          int term_cols = w.ws_col > 0 ? w.ws_col : 80;
          int max_width = 0;
          for(size_t i=0; i<menu_options.size(); i++) max_width = std::max(max_width, (int)menu_options[i].length());
          int cols = std::max(1, term_cols / (max_width + 2));

          if(seq[1] == 'C') menu_selected = (menu_selected + 1) % menu_options.size(); // RIGHT
          else if(seq[1] == 'D') menu_selected = (menu_selected - 1 + menu_options.size()) % menu_options.size(); // LEFT
          else if(seq[1] == 'B') menu_selected = (menu_selected + cols) % menu_options.size(); // DOWN
          else if(seq[1] == 'A') menu_selected = (menu_selected - cols + menu_options.size()) % menu_options.size(); // UP

          input = base_input_before_completion + menu_options[menu_selected];
          cursor_pos = input.length();
          redrawLine();
          continue;
        }

        if(seq[1] == 'A') { // up arrow ^[[A
          if(history_index > 0) { // check upto last command
            if(history_index == (int)command_history.size()) current_buffer = input;
            history_index--;
            input = command_history[history_index];
            cursor_pos = input.length();
            redrawLine();
          }
        } else if(seq[1] == 'B') { // down arrow ^[[B
          if(history_index < (int)command_history.size()) { // history_index is integer, and size() would return size_t, it won't allow negative, when compare that might problem
            history_index++;

            if(history_index == (int)command_history.size()) { // reach the end of the history list to show the empty string
              input = current_buffer;
            } else {
              input = command_history[history_index];
            }
            cursor_pos = input.length();
            redrawLine();
          }
        } else if(seq[1] == 'C') {
          if(cursor_pos < (int)input.length()) {
            cursor_pos++;
            redrawLine();
          } else if(!current_ghost_text.empty() && cursor_pos == (int)input.length()) {
            input += current_ghost_text;
            cursor_pos = input.length();
            redrawLine();
          }
        } else if(seq[1] == 'D') {
          if(cursor_pos > 0) {
            cursor_pos--;
            redrawLine();
          }
        }
      }
      tab_count = 0;
      continue;
    } else if(c == '\n') {
      if(in_menu) {
        closeMenu();
        continue;
      }
      std::string final_out = "\r\033[0J" + getBottomPrompt() + input + "\n";
      write(STDOUT_FILENO, final_out.c_str(), final_out.length()); // print the enter key
      break;
    } else if(c == 127) { // Backspace Key
      if(in_menu) closeMenu();
      if(cursor_pos > 0) {
        input.erase(cursor_pos - 1, 1);
        cursor_pos--;
        redrawLine();
      }
    } else if(c == '\t') {  // TAB key (Autocomplete)
      if(in_menu) {
        menu_selected = (menu_selected + 1) % menu_options.size();
        input = base_input_before_completion + menu_options[menu_selected];
        cursor_pos = input.length();
        redrawLine();
        continue;
      }

      if(input.empty() || input.find_first_not_of(' ') == std::string::npos) {
        write(STDOUT_FILENO, "\a", 1);
        tab_count = 0;
        continue;
      }

      tab_count++;
      std::string search_term = input;
      std::vector<std::string> matches;

      size_t last_space = input.find_last_of(' ');
      base_input_before_completion = (last_space != std::string::npos) ? input.substr(0, last_space + 1) : "";
      if(last_space != std::string::npos) { // if no space was there the it fails and do else
        search_term = input.substr(last_space + 1);
        std::string base_cmd = input.substr(0, input.find_first_of(' ')); // base_cmd like git, docker,

        std::string previous_word = "";
        size_t prev_end = input.find_last_not_of(' ', last_space); // find the character which is before the given string in reverse order

        if(prev_end != std::string::npos) {
          size_t prev_start = input.find_last_of(' ', prev_end); // 2nd parameter set the end value to find within it
          if(prev_start == std::string::npos) {
            prev_start = 0;
          } else {
            prev_start++;
          }
          previous_word = input.substr(prev_start, prev_end - prev_start + 1);
        }

        if(completion_scripts.find(base_cmd) != completion_scripts.end()) {
          matches = getProgrammableCompletions(completion_scripts[base_cmd], base_cmd, search_term, previous_word, input);
        } else {
          matches = getFileCompletions(search_term, base_cmd);
        }
      } else {
        if(search_term.find('/') != std::string::npos || search_term.find('.') == 0) {
          matches = getFileCompletions(search_term, search_term);
        } else {
          matches = getCompletions(search_term);
        }
      }

      // if found exactly one match, autocomplete it!
      if(matches.size() == 1) {
        // Grab the missing letters
        std::string completion = matches[0].substr(search_term.length());

        input += completion;
        // Print the missing letters to the screen
        cursor_pos = input.length();
        redrawLine();
      } else if(matches.size() > 1) {
        std::sort(matches.begin(), matches.end(), [](const std::string& a, const std::string& b) {
          bool a_is_dir = (!a.empty() && a.back() == '/');
          bool b_is_dir = (!b.empty() && b.back() == '/');
          if(a_is_dir != b_is_dir) return a_is_dir;
          return a < b;
        });

        std::string lcp = getLongestCommonPrefix(matches);

        if(lcp.length() > search_term.length()) {
          std::string added_chars = lcp.substr(search_term.length());  // starts from ith index to end.
          input += added_chars;
          cursor_pos = input.length();
          redrawLine();
        } else {
          in_menu = true;
          menu_options = matches;
          menu_selected = 0;
          viewport_start_row = 0;
          input = base_input_before_completion + menu_options[menu_selected];
          cursor_pos = input.length();
          redrawLine();
        }
      } else {
        write(STDOUT_FILENO, "\a", 1);
        tab_count = 0;
      }
    } else {
      if(in_menu) closeMenu();
      tab_count = 0;
      // Normal characters
      input.insert(cursor_pos, 1, c);
      cursor_pos++;
      redrawLine();
    }
  }

  // Restore normal Cooked Mode before running the command
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return true;
}

std::string buildPrompt() {
  std::string prompt = "";

  // get user and host
  const char* user = std::getenv("USER");
  char host[256];
  gethostname(host, sizeof(host));

  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  std::string cwd_str(cwd);
  const char* home = std::getenv("HOME");
  if(home != nullptr) {
    std::string home_str(home);
    if(cwd_str.find(home_str) == 0) {
      cwd_str.replace(0, home_str.length(), "~");
    }
  }

  bool is_root = (geteuid() == 0);
  std::string user_color = is_root ? "\033[1;31m" : "\033[1;32m";
  std::string user_str = user ? user : (is_root ? "root" : "user");
  

  // Assemble Top line 
  prompt += "\n\033[1;36m╭──(" + user_color + user_str + "\033[0m\033[1;34m🥷" + std::string(host) + "\033[1;36m)\033[0m ";
  prompt += "\033[1;33m" + cwd_str + "\033[0m\n";

  // Assemble Bottom line
  prompt += getBottomPrompt();

  return prompt;
}