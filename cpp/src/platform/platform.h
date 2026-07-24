#pragma once
#include <string>

namespace ocp {
namespace platform {

std::string save_file_dialog(const char* filter);
std::string open_file_dialog(const char* filter);
std::string get_app_data_dir();
std::string get_exe_dir();

} // namespace platform
} // namespace ocp
