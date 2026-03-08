#include "http/server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <exception>
#include <string>
#include <utility>

#include "http/logger.hpp"
#include "http/request.hpp"
#include "http/response.hpp"

namespace http {
namespace {

std::atomic<bool> g_stop_requested{false};

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
  response.headers["Connection"] = "close";  // keep-alive deferred to a future phase
  return response;
}

bool SendAll(int fd, const std::string& data) {
  std::size_t total_sent = 0;
  while (total_sent < data.size()) {
    const auto bytes_sent = send(fd, data.data() + total_sent, data.size() - total_sent, 0);
    if (bytes_sent < 0) {
      return false;
    }
    total_sent += static_cast<std::size_t>(bytes_sent);
  }
  return true;
}

std::string NormalizePath(std::string path) {
  const auto query_pos = path.find('?');
  if (query_pos != std::string::npos) {
    path = path.substr(0, query_pos);
  }

  if (path.empty()) {
    return "/";
  }
  return path;
}

}  // namespace

Server::Server(Config config)
    : config_(std::move(config)),
      static_file_handler_(config_.static_dir),
      thread_pool_(std::make_unique<ThreadPool>(config_.thread_count)) {
  router_.RegisterRoute("/", [](const HttpRequest&) {
    return MakeResponse(200, "OK", "text/plain; charset=utf-8", "HTTP server is running\n");
  });

  router_.RegisterRoute("/health", [](const HttpRequest&) {
    return MakeResponse(200, "OK", "application/json", "{\"status\":\"ok\"}\n");
  });

  router_.RegisterRoute("/hello", [](const HttpRequest&) {
    return MakeResponse(200, "OK", "text/plain; charset=utf-8", "Hello from C++ HTTP server\n");
  });
}

void Server::HandleClient(int client_fd, const std::string& client_ip, std::uint16_t client_port) {
  std::string raw_request;
  raw_request.reserve(4096);

  char buffer[4096];
  bool header_complete = false;
  while (raw_request.size() < 16 * 1024) {
    const auto bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
      break;
    }

    raw_request.append(buffer, static_cast<std::size_t>(bytes));
    if (raw_request.find("\r\n\r\n") != std::string::npos) {
      header_complete = true;
      break;
    }
  }

  HttpResponse response;
  HttpRequest request;
  std::string method = "?";
  std::string path = "?";

  try {
    if (!header_complete || !ParseHttpRequest(raw_request, &request)) {
      response = MakeResponse(400, "Bad Request", "text/plain; charset=utf-8", "400 Bad Request\n");
    } else {
      request.path = NormalizePath(request.path);
      method = request.method;
      path = request.path;

      if (request.method != "GET") {
        response =
            MakeResponse(400, "Bad Request", "text/plain; charset=utf-8", "Only GET is supported\n");
      } else if (request.path.rfind("/static", 0) == 0) {
        response = static_file_handler_.Serve(request.path);
      } else {
        response = router_.Route(request);
      }
    }
  } catch (const std::exception& ex) {
    Logger::Error(std::string("Unhandled exception while processing request: ") + ex.what());
    response = MakeResponse(500,
                            "Internal Server Error",
                            "text/plain; charset=utf-8",
                            "500 Internal Server Error\n");
  }

  if (!response.headers.count("Content-Type")) {
    response.headers["Content-Type"] = "text/plain; charset=utf-8";
  }
  response.headers["Connection"] = "close";

  const std::string wire_response = response.ToHttpString();
  if (!SendAll(client_fd, wire_response)) {
    Logger::Warn("send() failed while writing response");
  }

  Logger::Request(client_ip + ":" + std::to_string(client_port), method, path, response.status_code);
  close(client_fd);
}

void Server::Run() {
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

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
               " static_dir=" + config_.static_dir);

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
