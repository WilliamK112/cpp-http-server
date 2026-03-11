#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace http {

struct HttpRequest {
  std::string method;
  std::string path;    // normalized path without query string
  std::string target;  // raw request target (path + optional query)
  std::string version;
  std::unordered_map<std::string, std::string> headers;  // lower-case keys
  std::unordered_map<std::string, std::string> query_params;
  std::string body;
  std::string content_type;
};

// Parses one complete HTTP request from `raw_request`.
// Returns true on success and sets `consumed_bytes` to the exact request size.
// Returns false if malformed or incomplete.
bool ParseHttpRequest(const std::string& raw_request,
                      HttpRequest* out_request,
                      std::size_t* consumed_bytes = nullptr);

}  // namespace http
