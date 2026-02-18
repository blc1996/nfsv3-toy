#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "nfsv3/rpc.hpp"

namespace nfsv3 {

class Server {
 public:
  explicit Server(const std::filesystem::path &root);

  std::vector<uint8_t> handle_mount(const RpcCall &c);
  std::vector<uint8_t> handle_nfs(const RpcCall &c);

 private:
  std::filesystem::path export_root_;
  std::unordered_map<uint64_t, std::filesystem::path> handle_to_path_;
  std::mutex handle_mu_;
  uint64_t fsid_{0x12345678ULL};

  std::vector<uint8_t> make_fh(const std::filesystem::path &p);
  std::optional<std::filesystem::path> path_from_fh(Xdr &in);
  bool inside_root(const std::filesystem::path &p) const;
  bool write_fattr3(Xdr &o, const std::filesystem::path &p);
  void write_post_op_attr(Xdr &o, const std::filesystem::path &p);
  std::vector<uint8_t> nfs_status_only(uint32_t st);
};

}  // namespace nfsv3
