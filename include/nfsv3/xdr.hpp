#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nfsv3 {

struct Xdr {
  std::vector<uint8_t> buf;
  size_t off{0};

  explicit Xdr(std::vector<uint8_t> in = {});

  bool read_u32(uint32_t &v);
  bool read_u64(uint64_t &v);
  bool read_opaque(std::vector<uint8_t> &out);
  bool read_string(std::string &s);

  void write_u32(uint32_t v);
  void write_u64(uint64_t v);
  void write_opaque(std::span<const uint8_t> data);
  void write_string(const std::string &s);
};

}  // namespace nfsv3
