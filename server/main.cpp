#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "http_core.hpp"

namespace {

volatile std::sig_atomic_t g_running = 1;

void handle_signal(int) { g_running = 0; }

bool recv_until_complete(int client_fd, std::string &raw) {
  char buffer[4096];

  while (raw.find("\r\n\r\n") == std::string::npos) {
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) {
      return false;
    }
    raw.append(buffer, static_cast<size_t>(n));
    if (raw.size() > 1 << 20) {
      return false;
    }
  }

  const std::string marker = "\r\n\r\n";
  size_t header_end = raw.find(marker);
  size_t body_start = header_end + marker.size();

  size_t content_length = 0;
  size_t line_end = raw.find("\r\n");
  while (line_end != std::string::npos && line_end < header_end) {
    size_t next = raw.find("\r\n", line_end + 2);
    if (next == std::string::npos) {
      break;
    }
    std::string line = raw.substr(line_end + 2, next - (line_end + 2));
    if (line.empty()) {
      break;
    }
    size_t colon = line.find(':');
    if (colon != std::string::npos) {
      std::string name = line.substr(0, colon);
      std::string value = line.substr(colon + 1);
      if (!value.empty() && value[0] == ' ') {
        value.erase(0, 1);
      }
      if (name == "Content-Length") {
        try {
          content_length = static_cast<size_t>(std::stoul(value));
        }
        catch (...) {
          return false;
        }
      }
    }
    line_end = next;
  }

  while (raw.size() < body_start + content_length) {
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) {
      return false;
    }
    raw.append(buffer, static_cast<size_t>(n));
  }

  return true;
}

}  // namespace

int main(int argc, char **argv) {
  int port = 8080;
  if (argc > 1) {
    try {
      port = std::stoi(argv[1]);
    }
    catch (...) {
      std::cerr << "invalid port\n";
      return 1;
    }
  }

  std::signal(SIGINT, handle_signal);

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "socket failed: " << std::strerror(errno) << "\n";
    return 1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    std::cerr << "bind failed: " << std::strerror(errno) << "\n";
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 16) < 0) {
    std::cerr << "listen failed: " << std::strerror(errno) << "\n";
    close(server_fd);
    return 1;
  }

  std::cout << "listening on port " << port << "\n";

  while (g_running) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      std::cerr << "accept failed: " << std::strerror(errno) << "\n";
      break;
    }

    std::string raw;
    std::string response;
    if (!recv_until_complete(client_fd, raw)) {
      response = mini_http::make_response(400, "Bad Request", "incomplete request\n");
    }
    else {
      response = mini_http::handle_raw_request(raw);
    }

    send(client_fd, response.data(), response.size(), 0);
    close(client_fd);
  }

  close(server_fd);
  return 0;
}
