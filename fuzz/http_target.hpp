#ifndef FUZZ_HTTP_TARGET_HPP
#define FUZZ_HTTP_TARGET_HPP

#include <string>
#include <vector>

struct Header {
  std::string name;
  std::string value;
};

struct Request {
  std::string method;
  std::string path;
  std::string version;
  std::vector<Header> headers;
  std::string body;
  std::string query;
};

bool parse_request(const std::string &input, Request &request, std::string &error);
int process_request(const std::string &input);

void bug_null_deref();
void bug_divide_by_zero();
void bug_abort();
void bug_bad_function_pointer();
void bug_oob_stack_write();
void bug_use_after_free();

#endif
