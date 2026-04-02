#include "http_core.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

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

  const std::string mixed_marker = "\n\r\n";
  pos = input.find(mixed_marker);
  if (pos != std::string::npos) {
    headers_part = input.substr(0, pos);
    body = input.substr(pos + mixed_marker.size());
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

std::string trim_value(std::string value) {
  size_t start = 0;
  while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r')) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string header_value(const Request &request, const std::string &name) {
  auto it = request.headers.find(name);
  if (it != request.headers.end()) {
    return it->second;
  }
  return "";
}

int header_count(const Request &request, const std::string &name) {
  const std::string needle = lower_copy(name);
  int count = 0;
  for (const Header &header : request.header_list) {
    if (lower_copy(header.name) == needle) {
      ++count;
    }
  }
  return count;
}

bool has_query_flag(const Request &request, const std::string &needle) {
  return request.query.find(needle) != std::string::npos;
}

void bug_null_deref() {
  volatile int *ptr = nullptr;
  *ptr = 1337;
}

void bug_divide_by_zero() {
  volatile int zero = 0;
  volatile int value = 99 / zero;
  (void)value;
}

void bug_abort() { std::abort(); }

void bug_bad_function_pointer() {
  using Fn = void (*)();
  Fn fn = reinterpret_cast<Fn>(0x41);
  fn();
}

void bug_oob_stack_write() {
  volatile char buf[8] = {0};
  for (int i = 0; i < 64; ++i) {
    const_cast<char &>(buf[i]) = static_cast<char>(i);
  }
}

void bug_use_after_free() {
  int *ptr = new int[4];
  delete[] ptr;
  ptr[32] = 7;
  volatile int *null_ptr = nullptr;
  *null_ptr = ptr[32];
}

void handle_login(const Request &request) {
  const std::string auth = header_value(request, "Authorization");
  if (request.method == "POST" && auth.rfind("Basic ", 0) == 0 && request.body.find("letmein") != std::string::npos) {
    bug_null_deref();
  }

  if (header_count(request, "Host") >= 2 && has_query_flag(request, "fail=open-sesame")) {
    bug_abort();
  }
}

void handle_upload(const Request &request) {
  if (request.method == "POST" && header_value(request, "Content-Length") == "64" && request.body.rfind("MAGIC", 0) == 0) {
    bug_oob_stack_write();
  }

  if (header_value(request, "X-Debug") == "compress" && request.body.find("ZLIB") != std::string::npos) {
    bug_use_after_free();
  }
}

void handle_admin(const Request &request) {
  if (header_value(request, "X-Debug") == "1" && has_query_flag(request, "mode=canary")) {
    bug_bad_function_pointer();
  }

  if (header_value(request, "Authorization") == "Token zero" && request.body.find("0000") != std::string::npos) {
    bug_divide_by_zero();
  }
}

void handle_chunk(const Request &request) {
  if (lower_copy(header_value(request, "Transfer-Encoding")) == "chunked" && request.body.find("0;boom=") != std::string::npos) {
    bug_abort();
  }

  if (request.body.find("FFFFFFFF\r\n") != std::string::npos || request.body.find("FFFFFFFF\n") != std::string::npos) {
    bug_bad_function_pointer();
  }
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

  size_t query_pos = request.path.find('?');
  if (query_pos != std::string::npos) {
    request.query = request.path.substr(query_pos + 1);
    request.path = request.path.substr(0, query_pos);
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
    std::string name = trim_value(line.substr(0, colon));
    std::string value = trim_value(line.substr(colon + 1));
    request.header_list.push_back({name, value});
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
  if (request.method == "GET" && request.path == "/echo") {
    if (header_value(request, "X-Debug") == "reflect" && request.body.find("panic") != std::string::npos) {
      bug_null_deref();
    }
    return make_response(200, "OK", request.body);
  }

  if (request.path == "/login") {
    handle_login(request);
    return make_response(200, "OK", "login path reached\n");
  }

  if (request.path == "/upload") {
    handle_upload(request);
    return make_response(200, "OK", "upload path reached\n");
  }

  if (request.path == "/admin") {
    handle_admin(request);
    return make_response(200, "OK", "admin path reached\n");
  }

  if (request.path == "/chunk") {
    handle_chunk(request);
    return make_response(200, "OK", "chunk path reached\n");
  }

  if (header_count(request, "X-Route") >= 8 && has_query_flag(request, "trap=yes")) {
    bug_divide_by_zero();
  }

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
