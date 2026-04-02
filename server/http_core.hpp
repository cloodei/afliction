#ifndef SERVER_HTTP_CORE_HPP
#define SERVER_HTTP_CORE_HPP

#include <map>
#include <string>
#include <vector>

namespace mini_http {

struct Header {
  std::string name;
  std::string value;
};

struct Request {
  std::string method;
  std::string path;
  std::string version;
  std::map<std::string, std::string> headers;
  std::vector<Header> header_list;
  std::string body;
  std::string query;
};

bool parse_request(const std::string &raw, Request &request, std::string &error);
std::string make_response(int status, const std::string &reason, const std::string &body);
std::string handle_request(const Request &request);
std::string handle_raw_request(const std::string &raw);

}  // namespace mini_http

#endif
