#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <iostream>
#include <vector>

#include "nfsv3/config.hpp"
#include "nfsv3/constants.hpp"
#include "nfsv3/net.hpp"
#include "nfsv3/server.hpp"
#include "nfsv3/worker_pool.hpp"

int main(int argc, char **argv) {
  nfsv3::RuntimeConfig cfg;
  if (!nfsv3::parse_args(argc, argv, cfg)) return 0;

  std::error_code ec;
  std::filesystem::path canon = std::filesystem::weakly_canonical(cfg.export_dir, ec);
  if (ec || !std::filesystem::exists(canon) || !std::filesystem::is_directory(canon)) {
    std::cerr << "Export directory must exist and be a directory: " << cfg.export_dir << "\n";
    return 1;
  }

  nfsv3::Server server(canon);
  nfsv3::WorkerPool pool(cfg.max_workers);
  pool.set_server(&server);

  int nfs_fd = nfsv3::listen_tcp(cfg.nfs_port);
  int mnt_fd = nfsv3::listen_tcp(cfg.mount_port);
  if (nfs_fd < 0 || mnt_fd < 0) {
    std::perror("listen");
    return 1;
  }

  int epfd = epoll_create1(0);
  if (epfd < 0) {
    std::perror("epoll_create1");
    return 1;
  }

  if (!nfsv3::epoll_add(epfd, nfs_fd, EPOLLIN) || !nfsv3::epoll_add(epfd, mnt_fd, EPOLLIN)) {
    std::perror("epoll_ctl");
    return 1;
  }

  std::cout << "nfsv3_toy_server started\n"
            << "  export: " << canon << "\n"
            << "  mountd tcp port: " << cfg.mount_port << "\n"
            << "  nfs tcp port: " << cfg.nfs_port << "\n"
            << "  max workers: " << cfg.max_workers << "\n"
            << "Incoming sockets are accepted with epoll and queued to worker threads.\n"
            << "Use clients with: vers=3,tcp,port=" << cfg.nfs_port << ",mountport=" << cfg.mount_port
            << ",nolock\n";

  std::vector<epoll_event> events(16);
  while (true) {
    int ready = epoll_wait(epfd, events.data(), static_cast<int>(events.size()), -1);
    if (ready < 0) {
      if (errno == EINTR) continue;
      std::perror("epoll_wait");
      break;
    }

    for (int i = 0; i < ready; ++i) {
      int lfd = events[i].data.fd;
      uint32_t prog = (lfd == mnt_fd) ? nfsv3::MOUNT_PROGRAM : nfsv3::NFS_PROGRAM;
      uint32_t ver = (lfd == mnt_fd) ? nfsv3::MOUNT_V3 : nfsv3::NFS_V3;

      while (true) {
        int cfd = accept(lfd, nullptr, nullptr);
        if (cfd < 0) {
          if (errno == EINTR) continue;
          if (errno == EAGAIN || errno == EWOULDBLOCK) break;
          break;
        }
        pool.enqueue(nfsv3::Job{cfd, prog, ver});
      }
    }
  }

  ::close(epfd);
  ::close(nfs_fd);
  ::close(mnt_fd);
  return 0;
}
