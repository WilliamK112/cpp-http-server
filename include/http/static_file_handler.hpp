#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "http/response.hpp"

namespace http {

class StaticFileHandler {
 public:
  explicit StaticFileHandler(std::string root_dir);
  HttpResponse Serve(const std::string& request_path) const;

 private:
  struct CachedFile {
    std::string content_type;
    std::string body;
  };

  std::string root_dir_;

  // Small in-memory cache to avoid disk reads for hot static files.
  mutable std::mutex cache_mutex_;
  mutable std::unordered_map<std::string, CachedFile> cache_;
};

}  // namespace http
