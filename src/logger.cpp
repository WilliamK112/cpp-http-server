#include "http/logger.hpp"

#include <iostream>
#include <mutex>

namespace http {
namespace {

std::mutex g_log_mutex;

}  // namespace

void Logger::Info(const std::string& message) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::cout << "[INFO] " << message << std::endl;
}

void Logger::Warn(const std::string& message) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::cout << "[WARN] " << message << std::endl;
}

void Logger::Error(const std::string& message) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::cerr << "[ERROR] " << message << std::endl;
}

void Logger::Request(const std::string& client,
                     const std::string& method,
                     const std::string& path,
                     int status_code) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::cout << "[REQ] client=" << client << " method=" << method << " path=" << path
            << " status=" << status_code << std::endl;
}

}  // namespace http
