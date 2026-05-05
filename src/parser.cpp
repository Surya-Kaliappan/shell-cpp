#include "shell.h"

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