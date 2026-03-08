#pragma once

#include <string>
#include <unordered_map>

namespace http {

struct HttpResponse {
  int status_code{200};
  std::string status_text{"OK"};
  std::unordered_map<std::string, std::string> headers;
  std::string body;

  std::string ToHttpString() const;
};

}  // namespace http
