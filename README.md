# nfsv3-toy: C++ NFSv3 Server for Raspberry Pi USB Drives

This project is a **from-scratch C++ NFSv3 server implementation** (toy/minimal), intended for local-network use on a Raspberry Pi.

It serves one export directory (for example, a mounted USB hard drive) and exposes:

- `mountd` program (MOUNT v3) on a dedicated TCP port (default `20048`)
- `nfsd` program (NFS v3) on a dedicated TCP port (default `2049`)
- epoll-based accept loop + bounded worker pool (default `max-workers = max(4, hw_threads)`), so request handling is queued when workers are busy

> Important: this is not Linux `nfs-kernel-server` and not Ganesha. It is a custom C++ server in this repository.

## Current capability

Implemented NFSv3/MOUNT procedures are enough for common read-only browsing and file reads from Linux clients:

- MOUNT v3: `NULL`, `MNT`, `EXPORT`
- NFSv3: `NULL`, `GETATTR`, `LOOKUP`, `ACCESS`, `READ`, `READDIR`, `READDIRPLUS`, `FSSTAT`, `FSINFO`, `PATHCONF`

## Limitations

- Read-only behavior in practice (write/modify/create/remove not implemented).
- No RPCBIND/portmapper implementation; clients must specify `port` + `mountport` explicitly.
- No NLM/lockd; clients should mount with `nolock`.
- Minimal security/auth model (local trusted LAN only).
- This is a toy server; not suitable as a hardened production NAS server.

## Raspberry Pi setup

### 1) Prepare and mount your USB drive

Find the drive UUID and filesystem:

```bash
lsblk -f
```

Example mount point:

```bash
sudo mkdir -p /srv/usb-hdd
```

Add to `/etc/fstab` (example):

```fstab
UUID=<your-uuid> /srv/usb-hdd ext4 defaults,nofail 0 2
```

Mount it:

```bash
sudo mount -a
```

### 2) Build the C++ server

Install build tools:

```bash
sudo apt update
sudo apt install -y build-essential cmake
```

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Binary path:

```bash
./build/nfsv3_toy_server
```

### 3) Run the server on Raspberry Pi

```bash
sudo ./build/nfsv3_toy_server \
  --export /srv/usb-hdd \
  --nfs-port 2049 \
  --mount-port 20048 \
  --max-workers 8 \
  --verbose
```

The server listens on all interfaces (`0.0.0.0`), accepts connections using `epoll`, and queues accepted sockets to a fixed-size worker pool (`--max-workers`). Use `--verbose` to enable debug logs (RPC procedure traces, queue events, and connection lifecycle logs).

### 4) Open firewall (if enabled)

```bash
sudo ufw allow 2049/tcp
sudo ufw allow 20048/tcp
```

### 5) Mount from another Linux device

```bash
sudo mkdir -p /mnt/pi-usb
sudo mount -t nfs -o vers=3,tcp,port=2049,mountport=20048,nolock <pi-ip>:/srv/usb-hdd /mnt/pi-usb
```

Verify:

```bash
mount | grep /mnt/pi-usb
ls -la /mnt/pi-usb
```


## Logging

- Default mode logs important lifecycle and errors.
- Start with `--verbose` (or `-v`) to enable debug logs.
- Debug mode is useful for seeing queued request behavior and per-procedure handling details while testing on your LAN.

## Optional: systemd unit on Raspberry Pi

Create `/etc/systemd/system/nfsv3-toy.service`:

```ini
[Unit]
Description=Toy C++ NFSv3 server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/home/pi/nfsv3-toy/build/nfsv3_toy_server --export /srv/usb-hdd --nfs-port 2049 --mount-port 20048 --max-workers 8 --verbose
Restart=on-failure
User=root

[Install]
WantedBy=multi-user.target
```

Enable it:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now nfsv3-toy.service
sudo systemctl status nfsv3-toy.service
```

## Developer notes

This project is now modularized for easier extension:

- `src/main.cpp`: process entrypoint + epoll accept loop wiring
- `src/config.cpp`: CLI/runtime configuration parsing
- `src/net.cpp`: listener socket + epoll helper primitives
- `src/rpc.cpp`, `src/xdr.cpp`: RPC record and XDR encoding/decoding
- `src/server.cpp`: NFSv3 and MOUNTv3 procedure handling
- `src/worker_pool.cpp`: bounded worker queue and request execution
- `include/nfsv3/*.hpp`: interfaces/abstractions for each module
- `CMakeLists.txt`: build graph

Use this project as a learning base and extend it (write ops, stronger auth, rpcbind support, lock manager, better file-handle durability) as needed.


## Why epoll + worker pool instead of fork-per-connection?

- `fork()` per connection can create too many processes under load and has higher context-switch/memory overhead on Raspberry Pi.
- This server now uses a **bounded** thread pool, so concurrency is capped (`--max-workers`) and extra accepted sockets wait in a queue.
- `epoll` is used for scalable acceptance of sockets from both mountd and nfs listeners in one event loop.
- For this toy server, this is typically a better fit than unbounded process creation.
