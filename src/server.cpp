#include "http/server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <exception>
#include <sstream>
#include <string>
#include <utility>

#include "http/logger.hpp"
#include "http/request.hpp"
#include "http/response.hpp"

namespace http {
namespace {

std::atomic<bool> g_stop_requested{false};

enum class ReadStatus { kOk, kTimeout, kClientClosed, kTooLarge, kIoError };

void HandleSignal(int) { g_stop_requested.store(true); }

HttpResponse MakeResponse(int status_code,
                          std::string status_text,
                          std::string content_type,
                          std::string body) {
  HttpResponse response;
  response.status_code = status_code;
  response.status_text = std::move(status_text);
  response.body = std::move(body);
  response.headers["Content-Type"] = std::move(content_type);
  return response;
}

bool SendAll(int fd, const std::string& data) {
  std::size_t total_sent = 0;
  while (total_sent < data.size()) {
    const auto bytes_sent = send(fd, data.data() + total_sent, data.size() - total_sent, 0);
    if (bytes_sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    total_sent += static_cast<std::size_t>(bytes_sent);
  }
  return true;
}

std::string NormalizePath(std::string path) {
  if (path.empty()) {
    return "/";
  }
  return path;
}

bool SetSocketTimeoutMs(int fd, int timeout_ms) {
  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 &&
         setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
}

bool ShouldKeepAlive(const HttpRequest& request) {
  const auto it = request.headers.find("connection");
  std::string conn_value;
  if (it != request.headers.end()) {
    conn_value = it->second;
    std::transform(conn_value.begin(), conn_value.end(), conn_value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  }

  if (request.version == "HTTP/1.1") {
    if (it == request.headers.end()) {
      return true;
    }
    return conn_value != "close";
  }

  // HTTP/1.0 defaults to close unless explicitly keep-alive.
  return conn_value == "keep-alive";
}

std::size_t ExtractContentLengthFromHeaders(const std::string& header_block, bool* ok) {
  *ok = true;
  std::istringstream stream(header_block);
  std::string line;

  if (!std::getline(stream, line)) {
    return 0;
  }

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.empty()) {
      continue;
    }

    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, colon);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });

    if (key != "content-length") {
      continue;
    }

    try {
      return static_cast<std::size_t>(std::stoull(line.substr(colon + 1)));
    } catch (...) {
      *ok = false;
      return 0;
    }
  }

  return 0;
}

ReadStatus ReadOneRequest(int client_fd,
                          std::string* in_buffer,
                          std::string* out_raw_request,
                          const Config& config) {
  if (in_buffer == nullptr || out_raw_request == nullptr) {
    return ReadStatus::kIoError;
  }

  char buffer[4096];

  while (in_buffer->size() <= config.max_request_bytes) {
    const auto header_end = in_buffer->find("\r\n\r\n");
    if (header_end != std::string::npos) {
      const std::string headers_only = in_buffer->substr(0, header_end + 4);
      HttpRequest tmp_request;
      std::size_t consumed = 0;

      if (ParseHttpRequest(headers_only, &tmp_request, &consumed)) {
        *out_raw_request = in_buffer->substr(0, consumed);
        in_buffer->erase(0, consumed);
        return ReadStatus::kOk;
      }

      // We have headers, so parse content-length manually to know if body is still incomplete.
      bool content_length_ok = true;
      const std::size_t content_length = ExtractContentLengthFromHeaders(headers_only, &content_length_ok);
      if (!content_length_ok) {
        return ReadStatus::kIoError;
      }

      const std::size_t total_needed = header_end + 4 + content_length;
      if (total_needed > config.max_request_bytes || content_length > config.max_body_bytes) {
        return ReadStatus::kTooLarge;
      }

      if (in_buffer->size() >= total_needed) {
        *out_raw_request = in_buffer->substr(0, total_needed);
        in_buffer->erase(0, total_needed);
        return ReadStatus::kOk;
      }
    }

    const auto bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes == 0) {
      return ReadStatus::kClientClosed;
    }

    if (bytes < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        return ReadStatus::kTimeout;
      }
      return ReadStatus::kIoError;
    }

    in_buffer->append(buffer, static_cast<std::size_t>(bytes));
  }

  return ReadStatus::kTooLarge;
}

}  // namespace

Server::Server(Config config)
    : config_(std::move(config)),
      static_file_handler_(config_.static_dir),
      thread_pool_(std::make_unique<ThreadPool>(config_.thread_count)) {
  router_.RegisterRoute("GET", "/", [](const HttpRequest&) {
    return MakeResponse(200, "OK", "text/plain; charset=utf-8", "HTTP server is running\n");
  });

  router_.RegisterRoute("GET", "/health", [](const HttpRequest&) {
    return MakeResponse(200, "OK", "application/json", "{\"status\":\"ok\"}\n");
  });

  router_.RegisterRoute("GET", "/hello", [](const HttpRequest& request) {
    const auto it = request.query_params.find("name");
    const std::string name = it == request.query_params.end() ? "world" : it->second;
    return MakeResponse(200, "OK", "text/plain; charset=utf-8", "Hello, " + name + "\n");
  });

  router_.RegisterRoute("POST", "/echo", [](const HttpRequest& request) {
    const std::string type = request.content_type.empty() ? "text/plain; charset=utf-8" : request.content_type;
    return MakeResponse(200, "OK", type, request.body);
  });

  router_.RegisterRoute("PUT", "/resource", [](const HttpRequest& request) {
    return MakeResponse(200,
                        "OK",
                        "application/json",
                        "{\"updated\":true,\"bytes\":" + std::to_string(request.body.size()) + "}\n");
  });

  router_.RegisterRoute("DELETE", "/resource", [](const HttpRequest&) {
    return MakeResponse(204, "No Content", "text/plain; charset=utf-8", "");
  });
}

void Server::HandleClient(int client_fd, const std::string& client_ip, std::uint16_t client_port) {
  if (!SetSocketTimeoutMs(client_fd, config_.request_timeout_ms)) {
    Logger::Warn("Failed to configure socket timeouts for client");
  }

  std::string read_buffer;
  read_buffer.reserve(8192);

  std::size_t handled_requests = 0;
  bool close_connection = false;

  while (!close_connection && handled_requests < config_.max_requests_per_connection) {
    const int timeout_ms = handled_requests == 0 ? config_.request_timeout_ms : config_.keep_alive_timeout_ms;
    SetSocketTimeoutMs(client_fd, timeout_ms);

    std::string raw_request;
    const ReadStatus read_status = ReadOneRequest(client_fd, &read_buffer, &raw_request, config_);
    if (read_status == ReadStatus::kClientClosed || read_status == ReadStatus::kTimeout) {
      break;
    }

    HttpResponse response;
    HttpRequest request;
    std::string method = "?";
    std::string path = "?";

    try {
      if (read_status == ReadStatus::kTooLarge) {
        response = MakeResponse(413,
                                "Payload Too Large",
                                "text/plain; charset=utf-8",
                                "413 Payload Too Large\n");
        close_connection = true;
      } else if (read_status != ReadStatus::kOk || !ParseHttpRequest(raw_request, &request)) {
        response = MakeResponse(400, "Bad Request", "text/plain; charset=utf-8", "400 Bad Request\n");
        close_connection = true;
      } else {
        request.path = NormalizePath(request.path);
        method = request.method;
        path = request.path;

        if (request.path.rfind("/static", 0) == 0) {
          if (request.method != "GET") {
            response = MakeResponse(405,
                                    "Method Not Allowed",
                                    "text/plain; charset=utf-8",
                                    "405 Method Not Allowed\n");
            response.headers["Allow"] = "GET";
          } else {
            response = static_file_handler_.Serve(request.path);
          }
        } else {
          response = router_.Route(request);
        }

        close_connection = !ShouldKeepAlive(request);
      }
    } catch (const std::exception& ex) {
      Logger::Error(std::string("Unhandled exception while processing request: ") + ex.what());
      response = MakeResponse(500,
                              "Internal Server Error",
                              "text/plain; charset=utf-8",
                              "500 Internal Server Error\n");
      close_connection = true;
    }

    if (!response.headers.count("Content-Type")) {
      response.headers["Content-Type"] = "text/plain; charset=utf-8";
    }
    response.headers["Connection"] = close_connection ? "close" : "keep-alive";

    const std::string wire_response = response.ToHttpString();
    if (!SendAll(client_fd, wire_response)) {
      Logger::Warn("send() failed while writing response");
      break;
    }

    ++handled_requests;
    Logger::Request(client_ip + ":" + std::to_string(client_port), method, path, response.status_code);
  }

  close(client_fd);
}

void Server::Run() {
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);
  std::signal(SIGPIPE, SIG_IGN);

  const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    Logger::Error(std::string("socket() failed: ") + std::strerror(errno));
    return;
  }

  int enable = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
    Logger::Error(std::string("setsockopt(SO_REUSEADDR) failed: ") + std::strerror(errno));
    close(server_fd);
    return;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(config_.port);

  if (inet_pton(AF_INET, config_.host.c_str(), &address.sin_addr) <= 0) {
    Logger::Error("Invalid host: " + config_.host);
    close(server_fd);
    return;
  }

  if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    Logger::Error("bind() failed on " + config_.host + ":" + std::to_string(config_.port) +
                  " - " + std::strerror(errno));
    close(server_fd);
    return;
  }

  if (listen(server_fd, 128) < 0) {
    Logger::Error(std::string("listen() failed: ") + std::strerror(errno));
    close(server_fd);
    return;
  }

  Logger::Info("Listening on " + config_.host + ":" + std::to_string(config_.port) +
               " threads=" + std::to_string(config_.thread_count) +
               " static_dir=" + config_.static_dir +
               " request_timeout_ms=" + std::to_string(config_.request_timeout_ms) +
               " keep_alive_timeout_ms=" + std::to_string(config_.keep_alive_timeout_ms));

  while (!g_stop_requested.load()) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(server_fd, &read_set);

    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    const int ready = select(server_fd + 1, &read_set, nullptr, nullptr, &timeout);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      Logger::Warn(std::string("select() failed: ") + std::strerror(errno));
      continue;
    }

    if (ready == 0 || !FD_ISSET(server_fd, &read_set)) {
      continue;
    }

    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    const int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd < 0) {
      Logger::Warn(std::string("accept() failed: ") + std::strerror(errno));
      continue;
    }

    char ip_buffer[INET_ADDRSTRLEN] = {0};
    const char* ip = inet_ntop(AF_INET, &client_addr.sin_addr, ip_buffer, sizeof(ip_buffer));
    const std::string client_ip = ip ? ip : "unknown";
    const std::uint16_t client_port = ntohs(client_addr.sin_port);

    const bool queued =
        thread_pool_->Enqueue([this, client_fd, client_ip, client_port] {
          HandleClient(client_fd, client_ip, client_port);
        });

    if (!queued) {
      Logger::Warn("Thread pool is stopping; rejecting new connection");
      close(client_fd);
    }
  }

  Logger::Info("Shutdown requested. Closing listening socket.");
  close(server_fd);
}

}  // namespace http
