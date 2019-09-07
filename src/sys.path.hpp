// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_sys_path_hpp
#define contra_sys_path_hpp
#include <string>

namespace contra::sys {
  void initialize_path(int argc, const char** argv);

  std::string get_home_directory();
  std::string const& get_config_directory();
  std::string const& get_data_directory();
}

#endif
