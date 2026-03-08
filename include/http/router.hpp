#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "http/request.hpp"
#include "http/response.hpp"

namespace http {

using Handler = std::function<HttpResponse(const HttpRequest&)>;

class Router {
 public:
  void RegisterRoute(const std::string& path, Handler handler);
  HttpResponse Route(const HttpRequest& request) const;

 private:
  std::unordered_map<std::string, Handler> routes_;
};

}  // namespace http
