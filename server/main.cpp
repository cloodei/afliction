#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t g_running = 1;

struct Request {
  std::string method;
  std::string path;
  std::string version;
  std::map<std::string, std::string> headers;
  std::string body;
};

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

bool parse_request(const std::string &raw, Request &request, std::string &error) {
  const std::string marker = "\r\n\r\n";
  size_t header_end = raw.find(marker);
  if (header_end == std::string::npos) {
    error = "missing header terminator";
    return false;
  }

  std::string headers_part = raw.substr(0, header_end);
  std::istringstream stream(headers_part);
  std::string line;
  if (!std::getline(stream, line)) {
    error = "missing request line";
    return false;
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }

  std::istringstream request_line(line);
  if (!(request_line >> request.method >> request.path >> request.version)) {
    error = "bad request line";
    return false;
  }

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    size_t colon = line.find(':');
    if (colon == std::string::npos) {
      error = "bad header";
      return false;
    }
    std::string name = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    if (!value.empty() && value[0] == ' ') {
      value.erase(0, 1);
    }
    request.headers[name] = value;
  }

  if (request.version != "HTTP/1.1") {
    error = "only HTTP/1.1 is supported";
    return false;
  }
  if (request.headers.find("Host") == request.headers.end()) {
    error = "Host header required";
    return false;
  }

  request.body = raw.substr(header_end + marker.size());
  auto it = request.headers.find("Content-Length");
  if (it != request.headers.end()) {
    size_t expected = 0;
    try {
      expected = static_cast<size_t>(std::stoul(it->second));
    } catch (...) {
      error = "bad Content-Length";
      return false;
    }
    if (request.body.size() != expected) {
      error = "body length mismatch";
      return false;
    }
  }

  return true;
}

std::string make_response(int status, const std::string &reason, const std::string &body) {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << ' ' << reason << "\r\n";
  out << "Content-Type: text/plain\r\n";
  out << "Content-Length: " << body.size() << "\r\n";
  out << "Connection: close\r\n\r\n";
  out << body;
  return out.str();
}

std::string handle_request(const Request &request) {
  if (request.method == "GET" && request.path == "/")
    return make_response(200, "OK", "mini http server\n");
  if (request.method == "GET" && request.path == "/health")
    return make_response(200, "OK", "ok\n");
  if (request.method == "POST" && request.path == "/echo")
    return make_response(200, "OK", request.body);

  return make_response(404, "Not Found", "not found\n");
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
      response = make_response(400, "Bad Request", "incomplete request\n");
    }
    else {
      Request request;
      std::string error;
      if (!parse_request(raw, request, error)) {
        response = make_response(400, "Bad Request", error + "\n");
      }
      else {
        response = handle_request(request);
      }
    }

    send(client_fd, response.data(), response.size(), 0);
    close(client_fd);
  }

  close(server_fd);
  return 0;
}
