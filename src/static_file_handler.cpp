#include "http/static_file_handler.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace http {
namespace {

std::string GetMimeType(const std::filesystem::path& path) {
  static const std::unordered_map<std::string, std::string> kMime = {
      {".html", "text/html; charset=utf-8"},
      {".css", "text/css; charset=utf-8"},
      {".js", "application/javascript; charset=utf-8"},
      {".json", "application/json"},
      {".txt", "text/plain; charset=utf-8"},
  };

  const std::string ext = path.extension().string();
  const auto it = kMime.find(ext);
  return it == kMime.end() ? "application/octet-stream" : it->second;
}

bool IsSafeRelativePath(const std::filesystem::path& relative_path) {
  if (relative_path.is_absolute()) {
    return false;
  }

  for (const auto& part : relative_path) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

HttpResponse MakeError(int status_code, std::string status_text, std::string body) {
  return HttpResponse{status_code,
                      std::move(status_text),
                      {{"Content-Type", "text/plain; charset=utf-8"}},
                      std::move(body)};
}

}  // namespace

StaticFileHandler::StaticFileHandler(std::string root_dir) : root_dir_(std::move(root_dir)) {}

HttpResponse StaticFileHandler::Serve(const std::string& request_path) const {
  namespace fs = std::filesystem;

  try {
    std::string local = request_path;
    if (local.rfind("/static/", 0) == 0) {
      local = local.substr(8);  // remove /static/
    }

    if (local.empty()) {
      local = "index.html";
    }

    fs::path relative = fs::path(local).lexically_normal();
    if (!IsSafeRelativePath(relative)) {
      return MakeError(400, "Bad Request", "400 Bad Request\n");
    }

    const fs::path base = fs::weakly_canonical(fs::path(root_dir_));
    const fs::path full_path = fs::weakly_canonical(base / relative);

    const std::string base_str = base.string();
    const std::string full_str = full_path.string();
    if (full_str.rfind(base_str, 0) != 0) {
      return MakeError(400, "Bad Request", "400 Bad Request\n");
    }

    if (!fs::exists(full_path) || fs::is_directory(full_path)) {
      return MakeError(404, "Not Found", "404 Not Found\n");
    }

    const std::string cache_key = full_path.string();
    {
      std::lock_guard<std::mutex> lock(cache_mutex_);
      const auto it = cache_.find(cache_key);
      if (it != cache_.end()) {
        HttpResponse response;
        response.status_code = 200;
        response.status_text = "OK";
        response.headers["Content-Type"] = it->second.content_type;
        response.body = it->second.body;
        return response;
      }
    }

    std::ifstream file(full_path, std::ios::binary);
    if (!file) {
      return MakeError(500, "Internal Server Error", "500 Internal Server Error\n");
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    CachedFile cached{GetMimeType(full_path), buffer.str()};

    {
      std::lock_guard<std::mutex> lock(cache_mutex_);
      cache_[cache_key] = cached;
    }

    HttpResponse response;
    response.status_code = 200;
    response.status_text = "OK";
    response.body = std::move(cached.body);
    response.headers["Content-Type"] = std::move(cached.content_type);
    return response;
  } catch (...) {
    return MakeError(500, "Internal Server Error", "500 Internal Server Error\n");
  }
}

}  // namespace http
