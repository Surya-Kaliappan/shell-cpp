### codecrafter/shell/navigation/cd

created a builtin cd function to navigate through directories. and the structure of buildin list. and created the separate function for cd and pwd.

Complete cd functions with ~ , ../ 

---

### codecrafter/shell/quoting/single_quotes

Modified the function `parseCommand` to `parseInput` to store the value in the vector and used wisely. This reduced the some work in `executeExternal`.

### codecrafter/shell/quoting/double_quotes

Modified the function `parseInput` which accepts the double quotes and single quotes simultaneously.

### codecrafter/shell/quoting/backslash(outside)

Modified the function `parseInput` to add if condition at top of among all conditions. this escapes the character after the backslash.

### codecrafter/shell/quoting/backslash(inside)

No changes in code, becuase this backslash acts as other character in the input, no escaping special.

### codecrafter/shell/quoting/backslash(inside) double quotes

Lines extra added in the `parseInput` for backslash wintin the double quotes, this accepts the character after the backslash.

### codecrafter/shell/quoting/executable in quotes

No changes in code, because as usual, text in the double quotes would treat as a single token, so there is no functions, then i will search for executables.

---

### codecrafter/shell/redirection/stdout

This is the biggest change in the code, because before executing the input, it will check the name of file name first, if there then duplicate the current slot and save it in 4. after execting the input which saves the output in file, and back to original by changing the slot. here we used pipeline to change the data direction.

### codecrafter/shell/redirection/stderr

This is same as stdout, but this only stores the error when happen, for this we should mention as '2>'. This done by adding condition in `executeCommand`. created the 'target_fd' to target the slot accordingly.

### codecrafter/shell/redirection/append_stdout

Modified the `executeCommand` by tracking the appending mode to change the file flag. remaining things were work like `stdout` function.

### codecrafter/shell/redirection/append_stderr

Added a if condition which are already controlling the slot and flags in `executeCommand`.

---

### codecrafter/shell/command_completion/builtin_completion

Here the default `std::getline` has been removed, instead to get the input by function called `readLine` which is custom function that changes the behavious of terminal to raw mode from cooked mode. this raw mode won't have any functions to do. everything shold be control by code. once done with tab fuction, we back to the cooked mode to work as usual(after input).

### codecrafter/shell/command_completion/completion_with_argument

No changes done, because already mention that in `readLine`, once the enter hits, it gives \n which breaks the loop of getting input and start execute the input.

### codecrafter/shell/command_completion/missing_completion

No changes done. as usual, if there is no matcing foudn then won't auto complete the input. so already implemented in the function `readLine`

### codecrafter/shell/command_completion/executable_completion

A function named `getCompletion` which handles to collect the matching words and return in vector. this has some few line which is replaced from `readLine` which controll the builtin list. so now it `getCompletion` will handle both executable and builtin list to autocomplete. This gets the data from 'PATH' and check the executables.  

### codecrafter/shell/command_completion/multiple_completions

Modified the function `readLine` to count the tab pressed and add the condition to show the list of matches and again print the incomplete search text in the prompt input.

### codecrafter/shell/command_completion/partial_completion

Created a function named `getLongestCommonPrefix` which gives the longest common prefix return as string. after getting in `readLine` adds if condition to check the length of prefix and input should be greated. if greater then update the input, and STDOUT_FILENO for user visible. if not as usual it have old rest code of readline as else part.

---

### codecrafter/shell/pipelines/dual-command_pipeline

This is most confusing topic, here created a function called `executePipeline` which works like  `executeExternal` additional `executeCommand`. because combination of pid control and dup control in single function. here we separate the command into two, right and left, starts with left like feed to memory(write) and move to rigth like take from memory(read). In this function feels like most concepts understood from two functions were lies. for easy understand just dig a tunnel for both child one transfer the data which suppose to show in the output and another recieve data which suppose to input by user, this tunnel handles limited data(approx 64kb), so this all doing simutaneously.

### codecrafter/shell/pipeline/pipeline_with_builtins

This handles the to not every command should go through with execvp() so here created a function name `runWoker` which separates the flow for builtins to call its repective function, rest will be else part which is nothing but the part of both pid1 and pid2 in `executePipeline`. So nothing big deal.

### codecrafter/shell/pipeline/multi-command_pipelines

This dynamic approach would handle multiple command by like linked list connecting reading pipe to previous and writing pipe to next pipe. This changes the hard coded lines in function `executePipeline`. and in function `executeCommand` always checks the pipeline first, rest things after this.

---

### codecrafter/shell/history/history_builtin

This is most easiest thing, because here just added the vector to store every command input and function to print the history name `executeHistory`. and updated in the BUILTINS list.

### codecrafter/shell/history/listing_history

This was already done in previous stage.

### codecrafter/shell/history/limiting_history_entries

This changes the input of command, because here should handle arguments. to get this by stoi() function to convert from string to integer. after this we just change the logic of loop.

### codecrafter/shell/history/up-arrow_navigation

This changes happen in `readLine` to add extra condition which check the up arrow. if clicked then i take the last of history list, depends on the count click it will move on with index.

### codecrafter/shell/history/down-arrow_navigation

This is same as up arrow, just change the logic of index flow to flow next commands in history list. this also updated in `readLine` function.

### codecrafter/shell/history/executing_commands_from_history

This is already implemented, because the code architecture not only change the text of the command but also change the input value with manually, so when hit the enter it will execute the inner input and give result.
