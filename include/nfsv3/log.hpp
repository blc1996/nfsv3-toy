#pragma once

#include <string_view>

namespace nfsv3::log {

enum class Level { kInfo, kDebug, kError };

void set_verbose(bool enabled);
bool is_verbose();
void write(Level level, std::string_view message);

}  // namespace nfsv3::log
