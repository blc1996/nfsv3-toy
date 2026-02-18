#include "nfsv3/rpc.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include "nfsv3/constants.hpp"

namespace nfsv3 {

bool read_full(int fd, void *dst, size_t len) {
  auto *p = static_cast<uint8_t *>(dst);
  size_t done = 0;
  while (done < len) {
    ssize_t n = ::read(fd, p + done, len - done);
    if (n <= 0) return false;
    done += static_cast<size_t>(n);
  }
  return true;
}

bool write_full(int fd, const void *src, size_t len) {
  auto *p = static_cast<const uint8_t *>(src);
  size_t done = 0;
  while (done < len) {
    ssize_t n = ::write(fd, p + done, len - done);
    if (n <= 0) return false;
    done += static_cast<size_t>(n);
  }
  return true;
}

std::optional<std::vector<uint8_t>> read_rpc_record(int fd) {
  uint32_t hdr = 0;
  if (!read_full(fd, &hdr, sizeof(hdr))) return std::nullopt;
  hdr = ntohl(hdr);
  const uint32_t len = hdr & 0x7fffffffU;
  std::vector<uint8_t> data(len);
  if (len > 0 && !read_full(fd, data.data(), len)) return std::nullopt;
  return data;
}

bool write_rpc_record(int fd, const std::vector<uint8_t> &payload) {
  const uint32_t hdr = htonl(static_cast<uint32_t>(payload.size()) | 0x80000000U);
  return write_full(fd, &hdr, sizeof(hdr)) && (payload.empty() || write_full(fd, payload.data(), payload.size()));
}

bool parse_rpc_call(const std::vector<uint8_t> &msg, RpcCall &call) {
  Xdr x(msg);
  uint32_t msg_type = 0;
  uint32_t rpcvers = 0;
  if (!x.read_u32(call.xid) || !x.read_u32(msg_type) || !x.read_u32(rpcvers)) return false;
  if (msg_type != MSG_TYPE_CALL || rpcvers != RPC_VERSION) return false;
  if (!x.read_u32(call.prog) || !x.read_u32(call.vers) || !x.read_u32(call.proc)) return false;

  uint32_t cred_flavor = 0, cred_len = 0;
  if (!x.read_u32(cred_flavor) || !x.read_u32(cred_len)) return false;
  if (x.off + cred_len > x.buf.size()) return false;
  x.off += cred_len + ((4 - (cred_len % 4)) % 4);

  uint32_t verf_flavor = 0, verf_len = 0;
  if (!x.read_u32(verf_flavor) || !x.read_u32(verf_len)) return false;
  if (x.off + verf_len > x.buf.size()) return false;
  x.off += verf_len + ((4 - (verf_len % 4)) % 4);

  call.body.buf.assign(x.buf.begin() + static_cast<long>(x.off), x.buf.end());
  call.body.off = 0;
  return true;
}

std::vector<uint8_t> rpc_reply_header(uint32_t xid, uint32_t accept_stat) {
  Xdr out;
  out.write_u32(xid);
  out.write_u32(MSG_TYPE_REPLY);
  out.write_u32(REPLY_ACCEPTED);
  out.write_u32(AUTH_NULL);
  out.write_u32(0);
  out.write_u32(accept_stat);
  return out.buf;
}

}  // namespace nfsv3
