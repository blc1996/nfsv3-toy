#include "nfsv3/xdr.hpp"

namespace nfsv3 {

Xdr::Xdr(std::vector<uint8_t> in) : buf(std::move(in)) {}

bool Xdr::read_u32(uint32_t &v) {
  if (off + 4 > buf.size()) return false;
  v = (static_cast<uint32_t>(buf[off]) << 24) |
      (static_cast<uint32_t>(buf[off + 1]) << 16) |
      (static_cast<uint32_t>(buf[off + 2]) << 8) |
      static_cast<uint32_t>(buf[off + 3]);
  off += 4;
  return true;
}

bool Xdr::read_u64(uint64_t &v) {
  uint32_t hi = 0, lo = 0;
  if (!read_u32(hi) || !read_u32(lo)) return false;
  v = (static_cast<uint64_t>(hi) << 32) | lo;
  return true;
}

bool Xdr::read_opaque(std::vector<uint8_t> &out) {
  uint32_t len = 0;
  if (!read_u32(len)) return false;
  if (off + len > buf.size()) return false;
  out.assign(buf.begin() + static_cast<long>(off), buf.begin() + static_cast<long>(off + len));
  off += len;
  const size_t pad = (4 - (len % 4)) % 4;
  if (off + pad > buf.size()) return false;
  off += pad;
  return true;
}

bool Xdr::read_string(std::string &s) {
  std::vector<uint8_t> data;
  if (!read_opaque(data)) return false;
  s.assign(reinterpret_cast<const char *>(data.data()), data.size());
  return true;
}

void Xdr::write_u32(uint32_t v) {
  buf.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
  buf.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
  buf.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
  buf.push_back(static_cast<uint8_t>(v & 0xff));
}

void Xdr::write_u64(uint64_t v) {
  write_u32(static_cast<uint32_t>(v >> 32));
  write_u32(static_cast<uint32_t>(v & 0xffffffffULL));
}

void Xdr::write_opaque(std::span<const uint8_t> data) {
  write_u32(static_cast<uint32_t>(data.size()));
  buf.insert(buf.end(), data.begin(), data.end());
  const size_t pad = (4 - (data.size() % 4)) % 4;
  buf.insert(buf.end(), pad, 0);
}

void Xdr::write_string(const std::string &s) {
  write_opaque({reinterpret_cast<const uint8_t *>(s.data()), s.size()});
}

}  // namespace nfsv3
