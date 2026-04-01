#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "http_core.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <request-file>\n";
    return 1;
  }

  std::ifstream input(argv[1], std::ios::binary);
  if (!input) {
    std::cerr << "failed to open input\n";
    return 1;
  }

  std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::cout << mini_http::handle_raw_request(data);
  return 0;
}
