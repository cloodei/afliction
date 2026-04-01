CXX ?= g++
CXXFLAGS ?= -std=c++17 -O0 -g -Wall -Wextra -pedantic

BUILD_DIR := build
SERVER_BIN := $(BUILD_DIR)/mini_http_server
SERVER_HANDLER_BIN := $(BUILD_DIR)/mini_http_handler
FUZZ_BIN := $(BUILD_DIR)/mini_http_fuzz

SERVER_CORE_SRCS := server/http_core.cpp
SERVER_SRCS := server/main.cpp $(SERVER_CORE_SRCS)
SERVER_HANDLER_SRCS := server/harness.cpp $(SERVER_CORE_SRCS)
FUZZ_SRCS := fuzz/main.cpp fuzz/http_target.cpp fuzz/bugs.cpp

.PHONY: all server server-handler fuzz clean

all: server server-handler fuzz

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

server: $(SERVER_BIN)

server-handler: $(SERVER_HANDLER_BIN)

fuzz: $(FUZZ_BIN)

$(SERVER_BIN): $(SERVER_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_SRCS)

$(SERVER_HANDLER_BIN): $(SERVER_HANDLER_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_HANDLER_SRCS)

$(FUZZ_BIN): $(FUZZ_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(FUZZ_SRCS)

clean:
	rm -rf $(BUILD_DIR)
