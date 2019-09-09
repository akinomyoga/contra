#include <cstdio>
#include <cstdint>
#include <iostream>
#include <vector>

struct terminfo_header {
  std::int16_t magic;
  std::int16_t sz_name;
  std::int16_t n_bool;
  std::int16_t n_num;
  std::int16_t n_str;
  std::int16_t sz_str;
};

typedef unsigned char byte;

template<typename T>
bool read_vector(std::vector<T>& buffer, std::FILE* file) {
  return std::fread(&buffer[0], sizeof buffer[0], buffer.size(), file) == buffer.size();
}

bool read_file(const char* title, std::FILE* file) {
  // ToDo: assumes little endian

  std::size_t offset = 0;

  terminfo_header header;
  if (std::fread(&header, sizeof header, 1, file) != 1) {
    std::cerr << title << ": failed to read terminfo header." << std::endl;
    return false;
  }
  offset += sizeof header;

  if (header.magic != 0432) {
    std::cerr << title << ": invalid magic number in terminfo header" << std::endl;
    return false;
  }

  std::cout << "terminfo header" << "\n";
  std::cout << "  sz_name: " << header.sz_name << "\n";
  std::cout << "  n_bool: " << header.n_bool << "\n";
  std::cout << "  n_num: " << header.n_num << "\n";
  std::cout << "  n_str: " << header.n_str << "\n";
  std::cout << "  sz_str: " << header.sz_str << "\n";
  std::cout << std::flush;

  std::vector<char> terminal_name((std::size_t) header.sz_name);
  if (std::fread(&terminal_name[0], 1, header.sz_name, file) != header.sz_name) {
    std::cerr << title << " (name): unexpected termination of the file." << std::endl;
    return false;
  } else if (header.sz_name == 0 || terminal_name[header.sz_name - 1] != '\0') {
    std::cerr << title << " (name): not terminated by NUL." << std::endl;
    return false;
  }
  offset += header.sz_name;
  std::cout << "terminal_name: " << &terminal_name[0] << std::endl;

  std::vector<byte> bools((std::size_t) header.n_bool);
  if (!read_vector(bools, file)) {
    std::cerr << title << " (bool): unexpected termination of the file." << std::endl;
    return false;
  }
  offset += header.n_bool;
  if (offset & 1) {
    byte dummy;
    if (std::fread(&dummy, 1, 1, file) != 1) {
      std::cerr << title << " (bool): unexpected termination of the file." << std::endl;
      return false;
    } else if (dummy != 0) {
      std::cerr << title << " (bool): non-zero padding of bool section." << std::endl;
      return false;
    }
    offset++;
  }
  for (int i = 0; i < header.n_bool; i++)
    std::cout << "bool[" << i << "]: " << (int) bools[i] << "\n";

  std::vector<std::int16_t> nums((std::size_t) header.n_num);
  if (!read_vector(nums, file)) {
    std::cerr << title << " (num): unexpected termination of the file." << std::endl;
    return false;
  }
  for (int i = 0; i < header.n_num; i++) {
    if (nums[i] == -1)
      continue;
    else if (nums[i] < 0)
      std::cerr << "num[" << i << "]: invalid value" << "\n";
    else
      std::cout << "num[" << i << "]: " << nums[i] << "\n";
  }

  std::vector<std::int16_t> strs((std::size_t) header.n_str);
  if (!read_vector(strs, file)) {
    std::cerr << title << " (str): unexpected termination of the file." << std::endl;
    return false;
  }
  std::vector<char> table((std::size_t) header.sz_str);
  if (!read_vector(table, file)) {
    std::cerr << title << " (table): unexpected termination of the file." << std::endl;
    return false;
  }
  for (int i = 0; i < header.n_str; i++) {
    if (strs[i] == -1)
      continue;
    else if (strs[i] < 0 || header.sz_str <= strs[i])
      std::cerr << "str[" << i << "]: out of range" << "\n";
    else {
      std::string value = &table[strs[i]];
      std::cout << "str[" << i << "]: ";
      for (byte c : value) {
        if (c < 0x20) {
          switch (c) {
          case 0x1b: std::cout << "\\E"; break;
          case '\a': std::cout << "\\a"; break;
          case '\b': std::cout << "\\b"; break;
          case '\t': std::cout << "\\t"; break;
          case '\n': std::cout << "\\n"; break;
          case '\v': std::cout << "\\v"; break;
          case '\f': std::cout << "\\f"; break;
          case '\r': std::cout << "\\r"; break;
          default:
            std::cout << "^" << char(c + 64);
            break;
          }
        } else {
          if (c == '\\') {
            std::cout << "\\\\";
          } else if (c == '^') {
            std::cout << "\\^";
          } else {
            std::cout << (char) c;
          }
        }
      }
      std::cout << "\n";
    }
  }
}

int main() {
  const char* filename = "/usr/share/terminfo/63/cygwin";
  std::FILE* file = std::fopen(filename, "rb");
  read_file(filename, file);
  std::fclose(file);
  return 0;
}
