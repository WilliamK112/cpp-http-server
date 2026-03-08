#pragma once

#include <cstdint>
#include <string>

namespace http {

struct Config {
  std::string host{"127.0.0.1"};
  std::uint16_t port{8080};
  std::string static_dir{"public"};
  std::size_t thread_count{4};
};

Config ParseConfig(int argc, char* argv[]);

}  // namespace http
