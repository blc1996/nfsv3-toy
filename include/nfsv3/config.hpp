#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace nfsv3 {

struct RuntimeConfig {
  std::filesystem::path export_dir{"/srv/usb-hdd"};
  uint16_t nfs_port{2049};
  uint16_t mount_port{20048};
  size_t max_workers{4};
};

bool parse_args(int argc, char **argv, RuntimeConfig &cfg);

}  // namespace nfsv3
