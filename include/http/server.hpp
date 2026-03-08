#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "http/config.hpp"
#include "http/router.hpp"
#include "http/static_file_handler.hpp"
#include "http/thread_pool.hpp"

namespace http {

class Server {
 public:
  explicit Server(Config config);
  void Run();

 private:
  void HandleClient(int client_fd, const std::string& client_ip, std::uint16_t client_port);

  Config config_;
  Router router_;
  StaticFileHandler static_file_handler_;
  std::unique_ptr<ThreadPool> thread_pool_;
};

}  // namespace http
