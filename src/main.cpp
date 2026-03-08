#include "http/config.hpp"
#include "http/logger.hpp"
#include "http/server.hpp"

int main(int argc, char* argv[]) {
  const auto config = http::ParseConfig(argc, argv);

  http::Logger::Info("Starting C++ HTTP server");

  http::Server server(config);
  server.Run();

  return 0;
}
