CXX ?= g++
CXXFLAGS ?= -std=c++17 -O0 -g -Wall -Wextra -pedantic

BUILD_DIR := build
SERVER_BIN := $(BUILD_DIR)/mini_http_server
FUZZ_BIN := $(BUILD_DIR)/mini_http_fuzz

SERVER_SRCS := server/main.cpp
FUZZ_SRCS := fuzz/main.cpp fuzz/http_target.cpp fuzz/bugs.cpp

.PHONY: all server fuzz clean

all: server fuzz

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

server: $(SERVER_BIN)

fuzz: $(FUZZ_BIN)

$(SERVER_BIN): $(SERVER_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_SRCS)

$(FUZZ_BIN): $(FUZZ_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(FUZZ_SRCS)

clean:
	rm -rf $(BUILD_DIR)
