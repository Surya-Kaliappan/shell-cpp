# Compiler settings
CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17

# The final executable name
TARGET = a.out

# The source files we are going to create
SRCS = main.cpp parser.cpp builtins.cpp execution.cpp autocomplete.cpp ui.cpp config.cpp

# Convert .cpp names to .o (object) names
OBJS = $(SRCS:.cpp=.o)

# Default rule: typing 'make' runs this
all: $(TARGET)

# Linking the object files into the final executable
$(TARGET): $(OBJS)
		$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

#Compliling each .cpp file into a .o file
%.o: %.cpp shell.h
		$(CXX) $(CXXFLAGS) -c $< -o $@

# Typing 'make clean' deletes the compiled files so you can start fresh
clean:
		rm -f $(OBJS) $(TARGET)