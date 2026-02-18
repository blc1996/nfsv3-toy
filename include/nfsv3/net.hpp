#pragma once

#include <cstdint>

namespace nfsv3 {

int listen_tcp(uint16_t port);
bool epoll_add(int epfd, int fd, uint32_t events);

}  // namespace nfsv3
