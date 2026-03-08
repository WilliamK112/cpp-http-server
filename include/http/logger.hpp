#pragma once

#include <string>

namespace http {

class Logger {
 public:
  static void Info(const std::string& message);
  static void Warn(const std::string& message);
  static void Error(const std::string& message);
  static void Request(const std::string& client,
                      const std::string& method,
                      const std::string& path,
                      int status_code);
};

}  // namespace http
