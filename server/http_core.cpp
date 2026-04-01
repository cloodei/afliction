#include <sstream>

#include "http_core.hpp"

namespace mini_http {
namespace {

bool split_message(const std::string &input, std::string &headers_part, std::string &body) {
  const std::string crlf_marker = "\r\n\r\n";
  size_t pos = input.find(crlf_marker);
  if (pos != std::string::npos) {
    headers_part = input.substr(0, pos);
    body = input.substr(pos + crlf_marker.size());
    return true;
  }

  const std::string lf_marker = "\n\n";
  pos = input.find(lf_marker);
  if (pos != std::string::npos) {
    headers_part = input.substr(0, pos);
    body = input.substr(pos + lf_marker.size());
    return true;
  }

  return false;
}

std::string trim_line(std::string line) {
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  return line;
}

}  // namespace

bool parse_request(const std::string &raw, Request &request, std::string &error) {
  std::string headers_part;
  if (!split_message(raw, headers_part, request.body)) {
    error = "missing header terminator";
    return false;
  }

  std::istringstream stream(headers_part);
  std::string line;
  if (!std::getline(stream, line)) {
    error = "missing request line";
    return false;
  }
  line = trim_line(line);

  std::istringstream request_line(line);
  if (!(request_line >> request.method >> request.path >> request.version)) {
    error = "bad request line";
    return false;
  }

  while (std::getline(stream, line)) {
    line = trim_line(line);
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
  if (request.method == "GET" && request.path == "/") {
    return make_response(200, "OK", "mini http server\n");
  }
  if (request.method == "GET" && request.path == "/health") {
    return make_response(200, "OK", "ok\n");
  }
  if (request.method == "POST" && request.path == "/echo") {
    return make_response(200, "OK", request.body);
  }
  return make_response(404, "Not Found", "not found\n");
}

std::string handle_raw_request(const std::string &raw) {
  Request request;
  std::string error;
  if (!parse_request(raw, request, error)) {
    return make_response(400, "Bad Request", error + "\n");
  }
  return handle_request(request);
}

}  // namespace mini_http
