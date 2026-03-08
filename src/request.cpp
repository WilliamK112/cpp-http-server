#include "http/request.hpp"

#include <sstream>

namespace http {
namespace {

std::string Trim(const std::string& s) {
  const auto begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}

}  // namespace

bool ParseHttpRequest(const std::string& raw_request, HttpRequest* out_request) {
  if (out_request == nullptr || raw_request.empty()) {
    return false;
  }

  const auto header_end = raw_request.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return false;
  }

  const std::string header_block = raw_request.substr(0, header_end);
  std::istringstream stream(header_block);

  std::string request_line;
  if (!std::getline(stream, request_line)) {
    return false;
  }
  request_line = Trim(request_line);

  std::istringstream line_stream(request_line);
  HttpRequest request;
  if (!(line_stream >> request.method >> request.path >> request.version)) {
    return false;
  }

  if (request.version != "HTTP/1.1" && request.version != "HTTP/1.0") {
    return false;
  }

  std::string header_line;
  while (std::getline(stream, header_line)) {
    header_line = Trim(header_line);
    if (header_line.empty()) {
      continue;
    }

    const auto colon_pos = header_line.find(':');
    if (colon_pos == std::string::npos) {
      return false;
    }

    const std::string key = Trim(header_line.substr(0, colon_pos));
    const std::string value = Trim(header_line.substr(colon_pos + 1));
    if (key.empty()) {
      return false;
    }
    request.headers[key] = value;
  }

  request.body = raw_request.substr(header_end + 4);
  *out_request = std::move(request);
  return true;
}

}  // namespace http
