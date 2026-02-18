#include "nfsv3/config.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace nfsv3 {

bool parse_args(int argc, char **argv, RuntimeConfig &cfg) {
  cfg.max_workers = std::max<size_t>(4, std::thread::hardware_concurrency());

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](std::string_view opt) {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << opt << "\n";
        std::exit(2);
      }
      return std::string(argv[++i]);
    };

    if (a == "--export") {
      cfg.export_dir = need(a);
    } else if (a == "--nfs-port") {
      cfg.nfs_port = static_cast<uint16_t>(std::stoi(need(a)));
    } else if (a == "--mount-port") {
      cfg.mount_port = static_cast<uint16_t>(std::stoi(need(a)));
    } else if (a == "--max-workers") {
      cfg.max_workers = static_cast<size_t>(std::stoul(need(a)));
    } else if (a == "--verbose" || a == "-v") {
      cfg.verbose = true;
    } else if (a == "--help" || a == "-h") {
      std::cout << "Usage: nfsv3_toy_server [--export /path] [--nfs-port 2049] [--mount-port 20048] [--max-workers 8] [--verbose]\n";
      return false;
    } else {
      std::cerr << "Unknown argument: " << a << "\n";
      return false;
    }
  }

  if (cfg.max_workers == 0) cfg.max_workers = 1;
  return true;
}

}  // namespace nfsv3
