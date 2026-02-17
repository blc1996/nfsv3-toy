#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "nfsv3/xdr.hpp"

namespace nfsv3 {

struct RpcCall {
  uint32_t xid{};
  uint32_t prog{};
  uint32_t vers{};
  uint32_t proc{};
  Xdr body;
};

bool read_full(int fd, void *dst, size_t len);
bool write_full(int fd, const void *src, size_t len);

std::optional<std::vector<uint8_t>> read_rpc_record(int fd);
bool write_rpc_record(int fd, const std::vector<uint8_t> &payload);

bool parse_rpc_call(const std::vector<uint8_t> &msg, RpcCall &call);
std::vector<uint8_t> rpc_reply_header(uint32_t xid, uint32_t accept_stat);

}  // namespace nfsv3
