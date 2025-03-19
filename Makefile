# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -ISource

# Linker flags
LDFLAGS = -lavformat -lavcodec -lavutil -lsfml-graphics -lsfml-window -lsfml-system -lswscale -lswresample -lz -lm -lpthread

# Source directory
SRC_DIR = Source

# Automatically find all .cpp files in the source directory and its subdirectories
SOURCES = $(shell find $(SRC_DIR) -type f -iname "*.cpp")

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Executable name
TARGET = moon

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Phony targets
.PHONY: all clean
