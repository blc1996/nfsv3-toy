#include "nfsv3/log.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace nfsv3::log {
namespace {
std::mutex g_log_mu;
bool g_verbose = false;

const char *level_to_str(Level level) {
  switch (level) {
    case Level::kInfo:
      return "INFO";
    case Level::kDebug:
      return "DEBUG";
    case Level::kError:
      return "ERROR";
  }
  return "INFO";
}
}  // namespace

void set_verbose(bool enabled) {
  std::lock_guard<std::mutex> lk(g_log_mu);
  g_verbose = enabled;
}

bool is_verbose() {
  std::lock_guard<std::mutex> lk(g_log_mu);
  return g_verbose;
}

void write(Level level, std::string_view message) {
  if (level == Level::kDebug && !is_verbose()) return;

  std::lock_guard<std::mutex> lk(g_log_mu);
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  localtime_r(&t, &tm);
  std::cerr << std::put_time(&tm, "%F %T") << " [" << level_to_str(level) << "] " << message << "\n";
}

}  // namespace nfsv3::log
