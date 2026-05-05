#include "shell.h"
#include <termios.h>
#include <unistd.h>

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
        std::string base_cmd = input.substr(0, input.find_first_of(' '));

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
          matches = getFileCompletions(search_term);
        }
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