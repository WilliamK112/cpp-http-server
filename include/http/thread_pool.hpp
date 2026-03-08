#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace http {

class ThreadPool {
 public:
  explicit ThreadPool(std::size_t num_threads);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  bool Enqueue(std::function<void()> task);

 private:
  class TaskQueue {
   public:
    bool Push(std::function<void()> task);
    bool Pop(std::function<void()>* out_task);
    void Stop();

   private:
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_{false};
  };

  std::vector<std::thread> workers_;
  TaskQueue queue_;
};

}  // namespace http
