#include "http/request.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
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

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool ParseContentLength(const std::unordered_map<std::string, std::string>& headers,
                        std::size_t* out_len) {
  const auto it = headers.find("content-length");
  if (it == headers.end()) {
    *out_len = 0;
    return true;
  }

  try {
    const std::size_t parsed = std::stoull(Trim(it->second));
    *out_len = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

std::string UrlDecode(const std::string& s) {
  std::string out;
  out.reserve(s.size());

  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      const auto hex = s.substr(i + 1, 2);
      char* end = nullptr;
      const long value = std::strtol(hex.c_str(), &end, 16);
      if (end != nullptr && *end == '\0' && value >= 0 && value <= 255) {
        out.push_back(static_cast<char>(value));
        i += 2;
        continue;
      }
    }

    if (s[i] == '+') {
      out.push_back(' ');
    } else {
      out.push_back(s[i]);
    }
  }

  return out;
}

void ParseQueryParams(const std::string& query,
                      std::unordered_map<std::string, std::string>* out_params) {
  if (out_params == nullptr || query.empty()) {
    return;
  }

  std::size_t begin = 0;
  while (begin <= query.size()) {
    const auto end = query.find('&', begin);
    const std::string pair = query.substr(begin, end == std::string::npos ? std::string::npos : end - begin);

    if (!pair.empty()) {
      const auto eq = pair.find('=');
      if (eq == std::string::npos) {
        (*out_params)[UrlDecode(pair)] = "";
      } else {
        (*out_params)[UrlDecode(pair.substr(0, eq))] = UrlDecode(pair.substr(eq + 1));
      }
    }

    if (end == std::string::npos) {
      break;
    }
    begin = end + 1;
  }
}

}  // namespace

bool ParseHttpRequest(const std::string& raw_request,
                      HttpRequest* out_request,
                      std::size_t* consumed_bytes) {
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
  if (!(line_stream >> request.method >> request.target >> request.version)) {
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

    const std::string key = ToLower(Trim(header_line.substr(0, colon_pos)));
    const std::string value = Trim(header_line.substr(colon_pos + 1));
    if (key.empty()) {
      return false;
    }
    request.headers[key] = value;
  }

  std::size_t content_length = 0;
  if (!ParseContentLength(request.headers, &content_length)) {
    return false;
  }

  const std::size_t total_needed = header_end + 4 + content_length;
  if (raw_request.size() < total_needed) {
    return false;
  }

  request.body = raw_request.substr(header_end + 4, content_length);

  const auto query_pos = request.target.find('?');
  if (query_pos == std::string::npos) {
    request.path = request.target.empty() ? "/" : request.target;
  } else {
    request.path = request.target.substr(0, query_pos);
    ParseQueryParams(request.target.substr(query_pos + 1), &request.query_params);
  }

  if (request.path.empty()) {
    request.path = "/";
  }

  const auto ct_it = request.headers.find("content-type");
  if (ct_it != request.headers.end()) {
    request.content_type = ct_it->second;
  }

  if (consumed_bytes != nullptr) {
    *consumed_bytes = total_needed;
  }

  *out_request = std::move(request);
  return true;
}

}  // namespace http
