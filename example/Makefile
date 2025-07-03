# Makefile for HTTP Parse Library Examples

CXX = g++
CXXFLAGS_BASE = -std=c++17 -Wall -Wextra -I../include
CXXFLAGS_20 = -std=c++20 -Wall -Wextra -I../include
CXXFLAGS_23 = -std=c++23 -Wall -Wextra -I../include

# Output directory
BUILD_DIR = build

# Source files
SOURCES = basic_http1_parsing.cpp http2_connection.cpp version_detection.cpp incremental_parsing.cpp
SOURCES_CPP23 = cpp23_features.cpp

# Object files
OBJECTS = $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)
OBJECTS_CPP23 = $(SOURCES_CPP23:%.cpp=$(BUILD_DIR)/%.o)

# Executable names
TARGETS = $(SOURCES:%.cpp=$(BUILD_DIR)/%)
TARGETS_CPP23 = $(SOURCES_CPP23:%.cpp=$(BUILD_DIR)/%)

.PHONY: all clean cpp17 cpp20 cpp23 directories

all: directories cpp17

cpp17: directories $(TARGETS)
cpp20: directories $(TARGETS)
cpp23: directories $(TARGETS) $(TARGETS_CPP23)

directories:
	@mkdir -p $(BUILD_DIR)

# C++17 targets
$(BUILD_DIR)/basic_http1_parsing: basic_http1_parsing.cpp
	$(CXX) $(CXXFLAGS_BASE) -o $@ $<

$(BUILD_DIR)/http2_connection: http2_connection.cpp
	$(CXX) $(CXXFLAGS_BASE) -o $@ $<

$(BUILD_DIR)/version_detection: version_detection.cpp
	$(CXX) $(CXXFLAGS_BASE) -o $@ $<

$(BUILD_DIR)/incremental_parsing: incremental_parsing.cpp
	$(CXX) $(CXXFLAGS_BASE) -o $@ $<

# C++23 targets
$(BUILD_DIR)/cpp23_features: cpp23_features.cpp
	$(CXX) $(CXXFLAGS_23) -o $@ $<

# Run examples
run-basic: $(BUILD_DIR)/basic_http1_parsing
	@echo "Running basic HTTP/1 parsing example:"
	@./$(BUILD_DIR)/basic_http1_parsing

run-http2: $(BUILD_DIR)/http2_connection
	@echo "Running HTTP/2 connection example:"
	@./$(BUILD_DIR)/http2_connection

run-detection: $(BUILD_DIR)/version_detection
	@echo "Running version detection example:"
	@./$(BUILD_DIR)/version_detection

run-incremental: $(BUILD_DIR)/incremental_parsing
	@echo "Running incremental parsing example:"
	@./$(BUILD_DIR)/incremental_parsing

run-cpp23: $(BUILD_DIR)/cpp23_features
	@echo "Running C++23 features example:"
	@./$(BUILD_DIR)/cpp23_features

run-all: run-basic run-http2 run-detection run-incremental
	@echo "All examples completed!"

clean:
	rm -rf $(BUILD_DIR)

# Help target
help:
	@echo "HTTP Parse Library Examples"
	@echo "=========================="
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build all examples with C++17"
	@echo "  cpp17        - Build examples with C++17"
	@echo "  cpp20        - Build examples with C++20"
	@echo "  cpp23        - Build examples with C++23 (includes C++23 features)"
	@echo "  clean        - Remove build directory"
	@echo ""
	@echo "Run targets:"
	@echo "  run-basic    - Run basic HTTP/1 parsing example"
	@echo "  run-http2    - Run HTTP/2 connection example"
	@echo "  run-detection - Run version detection example"
	@echo "  run-incremental - Run incremental parsing example"
	@echo "  run-cpp23    - Run C++23 features example"
	@echo "  run-all      - Run all examples"
	@echo ""
	@echo "Usage examples:"
	@echo "  make cpp17 && make run-all"
	@echo "  make cpp23 && make run-cpp23"