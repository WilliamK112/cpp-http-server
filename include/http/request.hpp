#pragma once

#include <string>
#include <unordered_map>

namespace http {

struct HttpRequest {
  std::string method;
  std::string path;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

bool ParseHttpRequest(const std::string& raw_request, HttpRequest* out_request);

}  // namespace http
