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