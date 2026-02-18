#pragma once

#include <cstdint>

namespace nfsv3 {

constexpr uint32_t RPC_VERSION = 2;
constexpr uint32_t MSG_TYPE_CALL = 0;
constexpr uint32_t MSG_TYPE_REPLY = 1;
constexpr uint32_t REPLY_ACCEPTED = 0;
constexpr uint32_t ACCEPT_SUCCESS = 0;
constexpr uint32_t ACCEPT_PROC_UNAVAIL = 3;
constexpr uint32_t ACCEPT_GARBAGE_ARGS = 4;
constexpr uint32_t ACCEPT_PROG_UNAVAIL = 1;
constexpr uint32_t ACCEPT_PROG_MISMATCH = 2;

constexpr uint32_t AUTH_NULL = 0;

constexpr uint32_t MOUNT_PROGRAM = 100005;
constexpr uint32_t MOUNT_V3 = 3;
constexpr uint32_t NFS_PROGRAM = 100003;
constexpr uint32_t NFS_V3 = 3;

constexpr uint32_t MNT3_OK = 0;
constexpr uint32_t NFS3_OK = 0;
constexpr uint32_t NFS3ERR_NOENT = 2;
constexpr uint32_t NFS3ERR_IO = 5;
constexpr uint32_t NFS3ERR_ACCES = 13;
constexpr uint32_t NFS3ERR_NOTDIR = 20;
constexpr uint32_t NFS3ERR_INVAL = 22;
constexpr uint32_t NFS3ERR_STALE = 70;
constexpr uint32_t NFS3ERR_NOTSUPP = 10004;

constexpr uint32_t NF3REG = 1;
constexpr uint32_t NF3DIR = 2;
constexpr uint32_t NF3LNK = 5;

}  // namespace nfsv3
