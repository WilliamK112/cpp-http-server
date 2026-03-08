#include "http/router.hpp"

namespace http {

void Router::RegisterRoute(const std::string& path, Handler handler) {
  routes_[path] = std::move(handler);
}

HttpResponse Router::Route(const HttpRequest& request) const {
  const auto it = routes_.find(request.path);
  if (it == routes_.end()) {
    return HttpResponse{404, "Not Found", {{"Content-Type", "text/plain"}}, "404 Not Found\n"};
  }

  return it->second(request);
}

}  // namespace http
