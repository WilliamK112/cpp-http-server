#include "http/router.hpp"

#include <utility>

namespace http {

void Router::RegisterRoute(const std::string& path, Handler handler) {
  RegisterRoute("GET", path, std::move(handler));
}

void Router::RegisterRoute(const std::string& method, const std::string& path, Handler handler) {
  routes_[path][method] = std::move(handler);
}

HttpResponse Router::Route(const HttpRequest& request) const {
  const auto path_it = routes_.find(request.path);
  if (path_it == routes_.end()) {
    return HttpResponse{404, "Not Found", {{"Content-Type", "text/plain; charset=utf-8"}},
                        "404 Not Found\n"};
  }

  const auto method_it = path_it->second.find(request.method);
  if (method_it == path_it->second.end()) {
    std::string allow;
    for (const auto& entry : path_it->second) {
      if (!allow.empty()) {
        allow += ", ";
      }
      allow += entry.first;
    }

    return HttpResponse{405,
                        "Method Not Allowed",
                        {{"Content-Type", "text/plain; charset=utf-8"}, {"Allow", allow}},
                        "405 Method Not Allowed\n"};
  }

  return method_it->second(request);
}

}  // namespace http
