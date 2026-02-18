#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "nfsv3/server.hpp"

namespace nfsv3 {

struct Job {
  int client_fd;
  uint32_t expected_prog;
  uint32_t expected_ver;
};

class WorkerPool {
 public:
  explicit WorkerPool(size_t max_workers);
  ~WorkerPool();

  void enqueue(Job j);
  void set_server(Server *srv);

 private:
  void run();
  static void serve_client(int client, Server &srv, uint32_t expected_prog, uint32_t expected_ver);

  std::vector<std::thread> workers_;
  std::deque<Job> queue_;
  std::mutex mu_;
  std::condition_variable cv_;
  bool stopping_{false};
  Server *server_{nullptr};
};

}  // namespace nfsv3
