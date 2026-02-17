#include "nfsv3/server.hpp"

#include <sys/stat.h>

#include <filesystem>
#include <fstream>

#include "nfsv3/constants.hpp"

namespace nfsv3 {
namespace fs = std::filesystem;

static uint64_t hash_id(const fs::path &p) { return std::hash<std::string>{}(p.string()); }

Server::Server(const fs::path &root) : export_root_(fs::canonical(root)) { handle_to_path_[1] = export_root_; }

std::vector<uint8_t> Server::make_fh(const fs::path &p) {
  const fs::path canon = fs::weakly_canonical(p);
  const uint64_t id = (canon == export_root_) ? 1 : hash_id(canon);
  {
    std::lock_guard<std::mutex> lk(handle_mu_);
    handle_to_path_[id] = canon;
  }
  Xdr fh;
  fh.write_u64(id);
  return fh.buf;
}

std::optional<fs::path> Server::path_from_fh(Xdr &in) {
  std::vector<uint8_t> raw;
  if (!in.read_opaque(raw)) return std::nullopt;
  Xdr f(raw);
  uint64_t id = 0;
  if (!f.read_u64(id)) return std::nullopt;

  std::lock_guard<std::mutex> lk(handle_mu_);
  auto it = handle_to_path_.find(id);
  if (it == handle_to_path_.end()) return std::nullopt;
  return it->second;
}

bool Server::inside_root(const fs::path &p) const {
  const fs::path canon = fs::weakly_canonical(p);
  const auto root_str = export_root_.string();
  const auto pstr = canon.string();
  return pstr == root_str || (pstr.size() > root_str.size() && pstr.rfind(root_str + "/", 0) == 0);
}

bool Server::write_fattr3(Xdr &o, const fs::path &p) {
  std::error_code ec;
  auto st = fs::symlink_status(p, ec);
  if (ec) return false;

  uint32_t type = NF3REG;
  if (fs::is_directory(st)) type = NF3DIR;
  else if (fs::is_symlink(st)) type = NF3LNK;

  struct stat sb {};
  if (::lstat(p.c_str(), &sb) != 0) return false;

  o.write_u32(type);
  o.write_u32(static_cast<uint32_t>(sb.st_mode & 07777));
  o.write_u32(static_cast<uint32_t>(sb.st_nlink));
  o.write_u32(static_cast<uint32_t>(sb.st_uid));
  o.write_u32(static_cast<uint32_t>(sb.st_gid));
  o.write_u64(static_cast<uint64_t>(sb.st_size));
  o.write_u64(static_cast<uint64_t>(sb.st_blocks) * 512ULL);
  o.write_u32(0);
  o.write_u32(0);
  o.write_u64(fsid_);
  o.write_u64(static_cast<uint64_t>(sb.st_ino));
  o.write_u32(static_cast<uint32_t>(sb.st_atim.tv_sec));
  o.write_u32(static_cast<uint32_t>(sb.st_atim.tv_nsec));
  o.write_u32(static_cast<uint32_t>(sb.st_mtim.tv_sec));
  o.write_u32(static_cast<uint32_t>(sb.st_mtim.tv_nsec));
  o.write_u32(static_cast<uint32_t>(sb.st_ctim.tv_sec));
  o.write_u32(static_cast<uint32_t>(sb.st_ctim.tv_nsec));
  return true;
}

void Server::write_post_op_attr(Xdr &o, const fs::path &p) {
  o.write_u32(1);
  if (!write_fattr3(o, p)) {
    o.buf.resize(o.buf.size() - 4);
    o.write_u32(0);
  }
}

std::vector<uint8_t> Server::nfs_status_only(uint32_t st) {
  Xdr o;
  o.write_u32(st);
  return o.buf;
}

std::vector<uint8_t> Server::handle_mount(const RpcCall &c) {
  Xdr in = c.body;
  Xdr out;

  if (c.proc == 0) return out.buf;
  if (c.proc == 1) {
    std::string dir;
    if (!in.read_string(dir)) {
      out.write_u32(NFS3ERR_INVAL);
      return out.buf;
    }

    fs::path req = fs::weakly_canonical(dir);
    if (req != export_root_) {
      out.write_u32(NFS3ERR_ACCES);
      return out.buf;
    }

    out.write_u32(MNT3_OK);
    out.write_opaque(make_fh(export_root_));
    out.write_u32(1);
    out.write_u32(AUTH_NULL);
    return out.buf;
  }

  if (c.proc == 5) {
    out.write_u32(1);
    out.write_string(export_root_.string());
    out.write_u32(0);
    out.write_u32(0);
    return out.buf;
  }

  return {};
}

std::vector<uint8_t> Server::handle_nfs(const RpcCall &c) {
  Xdr in = c.body;
  Xdr out;

  if (c.proc == 0) return out.buf;

  if (c.proc == 1) {
    auto p = path_from_fh(in);
    if (!p) return nfs_status_only(NFS3ERR_STALE);
    out.write_u32(NFS3_OK);
    if (!write_fattr3(out, *p)) return nfs_status_only(NFS3ERR_IO);
    return out.buf;
  }

  if (c.proc == 3) {
    auto dir = path_from_fh(in);
    std::string name;
    if (!dir || !in.read_string(name)) return nfs_status_only(NFS3ERR_STALE);

    fs::path child = fs::weakly_canonical(*dir / name);
    if (!inside_root(child) || !fs::exists(child)) {
      out.write_u32(NFS3ERR_NOENT);
      write_post_op_attr(out, *dir);
      return out.buf;
    }

    out.write_u32(NFS3_OK);
    out.write_opaque(make_fh(child));
    write_post_op_attr(out, child);
    write_post_op_attr(out, *dir);
    return out.buf;
  }

  if (c.proc == 4) {
    auto p = path_from_fh(in);
    uint32_t req = 0;
    if (!p || !in.read_u32(req)) return nfs_status_only(NFS3ERR_STALE);
    out.write_u32(NFS3_OK);
    write_post_op_attr(out, *p);
    out.write_u32(req & 0x003fU);
    return out.buf;
  }

  if (c.proc == 6) {
    auto p = path_from_fh(in);
    uint64_t offset = 0;
    uint32_t count = 0;
    if (!p || !in.read_u64(offset) || !in.read_u32(count)) return nfs_status_only(NFS3ERR_STALE);
    if (fs::is_directory(*p)) return nfs_status_only(NFS3ERR_INVAL);

    std::ifstream file(*p, std::ios::binary);
    if (!file) return nfs_status_only(NFS3ERR_IO);
    file.seekg(0, std::ios::end);
    uint64_t size = static_cast<uint64_t>(file.tellg());
    if (offset > size) offset = size;
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

    std::vector<uint8_t> data(count);
    file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(count));
    const size_t got = static_cast<size_t>(file.gcount());
    data.resize(got);

    out.write_u32(NFS3_OK);
    write_post_op_attr(out, *p);
    out.write_u32(static_cast<uint32_t>(got));
    out.write_u32((offset + got) >= size ? 1 : 0);
    out.write_opaque(data);
    return out.buf;
  }

  if (c.proc == 16 || c.proc == 17) {
    auto dir = path_from_fh(in);
    uint64_t cookie = 0;
    uint64_t cookieverf = 0;
    uint32_t dircount = 0;
    uint32_t maxcount = 0;

    if (!dir || !in.read_u64(cookie) || !in.read_u64(cookieverf)) return nfs_status_only(NFS3ERR_STALE);
    if (c.proc == 16) {
      if (!in.read_u32(maxcount)) return nfs_status_only(NFS3ERR_INVAL);
    } else {
      if (!in.read_u32(dircount) || !in.read_u32(maxcount)) return nfs_status_only(NFS3ERR_INVAL);
    }

    if (!fs::is_directory(*dir)) return nfs_status_only(NFS3ERR_NOTDIR);

    std::vector<fs::directory_entry> ents;
    for (const auto &e : fs::directory_iterator(*dir)) ents.push_back(e);

    out.write_u32(NFS3_OK);
    write_post_op_attr(out, *dir);
    out.write_u64(0x42424242ULL);

    const size_t start = static_cast<size_t>(cookie);
    size_t sent = 0;
    for (size_t i = start; i < ents.size() && sent < 64; ++i, ++sent) {
      const auto name = ents[i].path().filename().string();
      struct stat sb {};
      if (::lstat(ents[i].path().c_str(), &sb) != 0) continue;

      out.write_u32(1);
      out.write_u64(static_cast<uint64_t>(sb.st_ino));
      out.write_string(name);
      out.write_u64(i + 1);

      if (c.proc == 17) {
        out.write_u32(1);
        write_fattr3(out, ents[i].path());
        out.write_u32(1);
        out.write_opaque(make_fh(ents[i].path()));
      }
    }

    out.write_u32(0);
    out.write_u32((start + sent) >= ents.size() ? 1 : 0);
    return out.buf;
  }

  if (c.proc == 18 || c.proc == 19 || c.proc == 20) {
    auto p = path_from_fh(in);
    if (!p) return nfs_status_only(NFS3ERR_STALE);

    out.write_u32(NFS3_OK);
    write_post_op_attr(out, *p);

    if (c.proc == 18) {
      out.write_u64(1024ULL * 1024ULL * 1024ULL);
      out.write_u64(1024ULL * 1024ULL * 1024ULL / 2ULL);
      out.write_u64(1024ULL * 1024ULL * 1024ULL / 2ULL);
      out.write_u64(1024ULL * 1024ULL);
      out.write_u64(1024ULL * 512ULL);
      out.write_u64(1024ULL * 512ULL);
      out.write_u32(0);
    } else if (c.proc == 19) {
      out.write_u32(4096);
      out.write_u32(4096);
      out.write_u32(1024 * 1024);
      out.write_u32(4096);
      out.write_u32(1024 * 1024);
      out.write_u32(1);
      out.write_u32(1);
      out.write_u32(0);
      out.write_u32(0x0003);
      out.write_u32(0);
    } else {
      out.write_u32(255);
      out.write_u32(255);
      out.write_u32(0);
      out.write_u32(1);
      out.write_u32(0);
      out.write_u32(1);
    }
    return out.buf;
  }

  return nfs_status_only(NFS3ERR_NOTSUPP);
}

}  // namespace nfsv3
