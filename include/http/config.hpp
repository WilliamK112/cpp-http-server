#pragma once

#include <cstdint>
#include <string>

namespace http {

struct Config {
  std::string host{"127.0.0.1"};
  std::uint16_t port{8080};
  std::string static_dir{"public"};
  std::size_t thread_count{4};

  int request_timeout_ms{5000};
  int keep_alive_timeout_ms{15000};
  std::size_t max_request_bytes{64 * 1024};
  std::size_t max_body_bytes{1024 * 1024};
  std::size_t max_requests_per_connection{100};
};

Config ParseConfig(int argc, char* argv[]);

}  // namespace http
