#include "http_target.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

bool split_message(const std::string &input, std::string &header_blob, std::string &body) {
  const std::string crlf_marker = "\r\n\r\n";
  size_t pos = input.find(crlf_marker);
  if (pos != std::string::npos) {
    header_blob = input.substr(0, pos);
    body = input.substr(pos + crlf_marker.size());
    return true;
  }

  const std::string lf_marker = "\n\n";
  pos = input.find(lf_marker);
  if (pos != std::string::npos) {
    header_blob = input.substr(0, pos);
    body = input.substr(pos + lf_marker.size());
    return true;
  }

  const std::string mixed_marker = "\n\r\n";
  pos = input.find(mixed_marker);
  if (pos != std::string::npos) {
    header_blob = input.substr(0, pos);
    body = input.substr(pos + mixed_marker.size());
    return true;
  }

  return false;
}

std::string trim(std::string value) {
  while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
    value.pop_back();
  }
  size_t start = 0;
  while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
    ++start;
  }
  return value.substr(start);
}

std::string lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string header_value(const Request &request, const std::string &name) {
  std::string needle = lower_copy(name);
  for (const Header &header : request.headers) {
    if (lower_copy(header.name) == needle) {
      return header.value;
    }
  }
  return "";
}

int header_count(const Request &request, const std::string &name) {
  std::string needle = lower_copy(name);
  int count = 0;
  for (const Header &header : request.headers) {
    if (lower_copy(header.name) == needle) {
      ++count;
    }
  }
  return count;
}

bool has_query_flag(const Request &request, const std::string &flag) {
  return request.query.find(flag) != std::string::npos;
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
  const std::string length = header_value(request, "Content-Length");
  if (request.method == "POST" && length == "64" && request.body.rfind("MAGIC", 0) == 0) {
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

  if (request.body.find("FFFFFFFF\r\n") != std::string::npos) {
    bug_bad_function_pointer();
  }
}

}  // namespace

bool parse_request(const std::string &input, Request &request, std::string &error) {
  std::string header_blob;
  if (!split_message(input, header_blob, request.body)) {
    error = "missing header terminator";
    return false;
  }

  std::istringstream stream(header_blob);
  std::string line;
  if (!std::getline(stream, line)) {
    error = "missing request line";
    return false;
  }
  line = trim(line);

  std::istringstream first(line);
  if (!(first >> request.method >> request.path >> request.version)) {
    error = "bad request line";
    return false;
  }

  size_t query_pos = request.path.find('?');
  if (query_pos != std::string::npos) {
    request.query = request.path.substr(query_pos + 1);
    request.path = request.path.substr(0, query_pos);
  }

  if (request.version != "HTTP/1.1") {
    error = "unsupported version";
    return false;
  }

  while (std::getline(stream, line)) {
    line = trim(line);
    if (line.empty()) {
      continue;
    }
    size_t colon = line.find(':');
    if (colon == std::string::npos) {
      error = "malformed header";
      return false;
    }
    Header header;
    header.name = trim(line.substr(0, colon));
    header.value = trim(line.substr(colon + 1));
    request.headers.push_back(header);
  }

  if (header_count(request, "Host") == 0) {
    error = "host required";
    return false;
  }

  const std::string content_length = header_value(request, "Content-Length");
  if (!content_length.empty()) {
    try {
      size_t expected = static_cast<size_t>(std::stoul(content_length));
      if (request.body.size() != expected) {
        error = "body size mismatch";
        return false;
      }
    } catch (...) {
      error = "invalid content length";
      return false;
    }
  }

  return true;
}

int process_request(const std::string &input) {
  Request request;
  std::string error;
  if (!parse_request(input, request, error)) {
    return 1;
  }

  if (request.method == "GET" && request.path == "/echo") {
    if (header_value(request, "X-Debug") == "reflect" && request.body.find("panic") != std::string::npos) {
      bug_null_deref();
    }
    return 0;
  }

  if (request.path == "/login") {
    handle_login(request);
    return 0;
  }

  if (request.path == "/upload") {
    handle_upload(request);
    return 0;
  }

  if (request.path == "/admin") {
    handle_admin(request);
    return 0;
  }

  if (request.path == "/chunk") {
    handle_chunk(request);
    return 0;
  }

  if (header_count(request, "X-Route") >= 8 && has_query_flag(request, "trap=yes")) {
    bug_divide_by_zero();
  }

  return 0;
}
