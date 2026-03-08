#include "http/thread_pool.hpp"

#include <utility>

namespace http {

bool ThreadPool::TaskQueue::Push(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return false;
    }
    tasks_.push(std::move(task));
  }

  cv_.notify_one();
  return true;
}

bool ThreadPool::TaskQueue::Pop(std::function<void()>* out_task) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });

  if (tasks_.empty()) {
    return false;
  }

  *out_task = std::move(tasks_.front());
  tasks_.pop();
  return true;
}

void ThreadPool::TaskQueue::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  cv_.notify_all();
}

ThreadPool::ThreadPool(std::size_t num_threads) {
  if (num_threads == 0) {
    num_threads = 1;
  }

  workers_.reserve(num_threads);
  for (std::size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] {
      while (true) {
        std::function<void()> task;
        if (!queue_.Pop(&task)) {
          return;
        }
        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  queue_.Stop();

  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

bool ThreadPool::Enqueue(std::function<void()> task) { return queue_.Push(std::move(task)); }

}  // namespace http
