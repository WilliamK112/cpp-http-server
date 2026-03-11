#include "http/config.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace http {
namespace {

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

}  // namespace

Config ParseConfig(int argc, char* argv[]) {
  Config config;

  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);

    try {
      if (StartsWith(arg, "--port=")) {
        const int port = std::stoi(arg.substr(7));
        if (port > 0 && port <= 65535) {
          config.port = static_cast<std::uint16_t>(port);
        } else {
          std::cerr << "[warn] Ignoring invalid port: " << port << std::endl;
        }
      } else if (StartsWith(arg, "--threads=")) {
        const int threads = std::stoi(arg.substr(10));
        if (threads > 0) {
          config.thread_count = static_cast<std::size_t>(threads);
        } else {
          std::cerr << "[warn] Ignoring invalid thread count: " << threads << std::endl;
        }
      } else if (StartsWith(arg, "--static-dir=")) {
        config.static_dir = arg.substr(13);
      } else if (StartsWith(arg, "--host=")) {
        config.host = arg.substr(7);
      } else if (StartsWith(arg, "--request-timeout-ms=")) {
        const int timeout = std::stoi(arg.substr(21));
        if (timeout > 0) {
          config.request_timeout_ms = timeout;
        }
      } else if (StartsWith(arg, "--keep-alive-timeout-ms=")) {
        const int timeout = std::stoi(arg.substr(24));
        if (timeout > 0) {
          config.keep_alive_timeout_ms = timeout;
        }
      } else if (StartsWith(arg, "--max-request-bytes=")) {
        const int bytes = std::stoi(arg.substr(20));
        if (bytes > 0) {
          config.max_request_bytes = static_cast<std::size_t>(bytes);
        }
      } else if (StartsWith(arg, "--max-body-bytes=")) {
        const int bytes = std::stoi(arg.substr(17));
        if (bytes >= 0) {
          config.max_body_bytes = static_cast<std::size_t>(bytes);
        }
      } else if (StartsWith(arg, "--max-requests-per-connection=")) {
        const int n = std::stoi(arg.substr(30));
        if (n > 0) {
          config.max_requests_per_connection = static_cast<std::size_t>(n);
        }
      }
    } catch (const std::exception& ex) {
      std::cerr << "[warn] Ignoring invalid argument '" << arg << "': " << ex.what() << std::endl;
    }
  }

  return config;
}

}  // namespace http
