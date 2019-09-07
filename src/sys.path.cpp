#include "sys.path.hpp"
#include <string>
#include <vector>
#include <mwg/except.h>

// stat
#include <sys/types.h>
#include <sys/stat.h>

// readlink
#include <unistd.h>

// getuid, getpwuid
#include <pwd.h>

namespace contra::sys {

  static std::string contra_binary_path;
  static std::string contra_binary_directory;
  static std::string contra_data_directory;
  static std::string contra_config_directory;

  std::string get_directory(std::string const& path) {
    std::size_t const index = path.find_last_of("/");
    if (index == std::string::npos)
      return ".";
    else
      return path.substr(0, index);
  }
  std::string get_filename(std::string const& path) {
    std::size_t const index = path.find_last_of("/");
    if (index == std::string::npos)
      return path;
    else
      return path.substr(index + 1);
  }
  bool is_absolute_path(const char* path) {
    return path[0] == '/';
  }
  bool is_directory(const char* path) {
    struct stat st;
    if (::stat(path, &st) != 0) return false;
    return st.st_mode & S_IFDIR;
  }

  static ssize_t readlink(const char* path, std::vector<char>& buff) {
    for (;;) {
      ssize_t sz = ::readlink(path, &buff[0], buff.size());
      if (sz < 0) {
        return -1;
      } else if ((std::size_t) sz < buff.size()) {
        buff[sz] = '\0';
        return sz;
      }
      buff.resize(buff.size() * 2);
    }
  }
  std::string resolve_link(const char* path) {
    std::string ret = path;
    std::vector<char> buff((std::size_t) 100);
    struct stat st;
    ::stat(path, &st);
    while ((st.st_mode & S_IFLNK) && readlink(ret.c_str(), buff) > 0) {
      if (is_absolute_path(&buff[0]))
        ret = &buff[0];
      else
        ret = get_directory(ret) + "/" + &buff[0];
    }
    return ret;
  }

  std::string get_home_directory() {
    struct passwd* pw = ::getpwuid(::getuid());
    if (pw && pw->pw_dir)
      return pw->pw_dir;
    return std::getenv("HOME");
  }

  static void initialize_data_directory(std::string& ret) {
    const char* data_home = std::getenv("XDG_DATA_HOME");
    if (data_home && *data_home) {
      ret = data_home;
      ret += "/contra";
      if (is_directory(ret.c_str())) return;
    }

    ret = get_home_directory();
    if (!ret.empty()) {
      if (ret.back() != '/') ret += '/';
      ret += ".local/share/contra";
      if (is_directory(ret.c_str())) return;
    }

    if (!contra_binary_directory.empty()) {
      if (get_filename(contra_binary_directory) == "bin") {
        ret = get_directory(contra_binary_directory) + "/share/contra";
        if (is_directory(ret.c_str())) return;
      }

      ret = contra_binary_directory + "/res";
      if (is_directory(ret.c_str())) return;
    }

    ret = "res";
  }

  static void initialize_config_directory(std::string& ret) {
    const char* config_home = std::getenv("XDG_CONFIG_HOME");
    if (config_home && *config_home) {
      ret = config_home;
      ret += "/contra";
      if (is_directory(ret.c_str())) return;
    }

    ret = get_home_directory();
    if (!ret.empty() && ret.back() != '/') ret += '/';
    ret += ".config/contra";
  }

  std::string const& get_config_directory() { return contra_config_directory; }
  std::string const& get_data_directory() { return contra_data_directory; }

  void initialize_path(int argc, const char** argv) {
    if (argc >= 1) {
      contra_binary_path = argv[0];
      contra_binary_directory = get_directory(resolve_link(argv[0]));
    }

    initialize_data_directory(contra_data_directory);
    initialize_config_directory(contra_config_directory);
  }
}
