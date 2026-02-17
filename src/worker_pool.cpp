#include "nfsv3/worker_pool.hpp"

#include <unistd.h>

#include "nfsv3/constants.hpp"
#include "nfsv3/rpc.hpp"

namespace nfsv3 {

WorkerPool::WorkerPool(size_t max_workers) {
  if (max_workers == 0) max_workers = 1;
  workers_.reserve(max_workers);
  for (size_t i = 0; i < max_workers; ++i) {
    workers_.emplace_back([this] { run(); });
  }
}

WorkerPool::~WorkerPool() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    stopping_ = true;
  }
  cv_.notify_all();
  for (auto &t : workers_) t.join();
}

void WorkerPool::set_server(Server *srv) { server_ = srv; }

void WorkerPool::enqueue(Job j) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    queue_.push_back(j);
  }
  cv_.notify_one();
}

void WorkerPool::run() {
  while (true) {
    Job j{-1, 0, 0};
    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_.wait(lk, [this] { return stopping_ || !queue_.empty(); });
      if (stopping_ && queue_.empty()) return;
      j = queue_.front();
      queue_.pop_front();
    }
    serve_client(j.client_fd, *server_, j.expected_prog, j.expected_ver);
  }
}

void WorkerPool::serve_client(int client, Server &srv, uint32_t expected_prog, uint32_t expected_ver) {
  while (true) {
    auto rec = read_rpc_record(client);
    if (!rec) break;

    RpcCall c;
    if (!parse_rpc_call(*rec, c)) {
      auto r = rpc_reply_header(0, ACCEPT_GARBAGE_ARGS);
      write_rpc_record(client, r);
      continue;
    }

    if (c.prog != expected_prog) {
      auto payload = rpc_reply_header(c.xid, ACCEPT_PROG_UNAVAIL);
      write_rpc_record(client, payload);
      continue;
    }

    std::vector<uint8_t> hdr;
    if (c.vers != expected_ver) {
      hdr = rpc_reply_header(c.xid, ACCEPT_PROG_MISMATCH);
      Xdr x(hdr);
      x.buf = hdr;
      x.write_u32(expected_ver);
      x.write_u32(expected_ver);
      write_rpc_record(client, x.buf);
      continue;
    }

    hdr = rpc_reply_header(c.xid, ACCEPT_SUCCESS);
    std::vector<uint8_t> body;
    if (expected_prog == MOUNT_PROGRAM) {
      body = srv.handle_mount(c);
      if (body.empty() && c.proc != 0 && c.proc != 1 && c.proc != 5) {
        hdr = rpc_reply_header(c.xid, ACCEPT_PROC_UNAVAIL);
      }
    } else {
      body = srv.handle_nfs(c);
    }

    hdr.insert(hdr.end(), body.begin(), body.end());
    if (!write_rpc_record(client, hdr)) break;
  }
  ::close(client);
}

}  // namespace nfsv3
