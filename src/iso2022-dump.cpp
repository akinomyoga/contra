#include <cctype>
#include <cstring>
#include <iterator>
#include <iomanip>
#include <utility>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "enc.utf8.hpp"

char32_t unused = 0xFFFFFFFF;

enum charset_type {
  charset_sb94,
  charset_sb96,
  charset_mb94,
  charset_mb96,
};

struct sb94data {
  int type;
  std::string seq;
  std::string reg;
  std::string name;

  char32_t map[128];
  sb94data() {
    std::fill(std::begin(map), std::end(map), unused);
  }
};

struct processor_t {
  std::unordered_map<std::string, sb94data> charset94;

  int iline = 0;
  char32_t const* r;
  char32_t const* r0;
  char32_t const* rN;

  sb94data* current_charset_data = nullptr;

  bool skip_space() {
    if (!std::isspace(*r)) return false;
    while (std::isspace(*++r));
    return true;
  }
  bool starts_with_skip(const char* head) {
    std::size_t const sz = std::strlen(head);
    if (!(r + sz <= rN)) return false;
    for (std::size_t i = 0; i < sz; i++)
      if (r[i] != (char32_t) head[i]) return false;
    r += sz;
    return true;
  }
  bool read_until(char32_t c, std::string& buff) {
    while (*r && *r != c) buff += *r++;
    return *r == c;
  }

  std::ostream& print_error() {
    return std::cerr << "line:" << iline << ":" << r - r0 << ": ";
  }
  unsigned read_unsigned() {
    unsigned ret = 0;
    while (std::isdigit(*r))
      ret = ret * 10 + (*r++ - '0');
    return ret;
  }
  static int hex2int(char32_t u) {
    if (std::isdigit(u)) return u - '0';
    if (std::islower(u)) return u - 'a' + 10;
    if (std::isupper(u)) return u - 'A' + 10;
    return -1;
  }
  unsigned read_hex() {
    unsigned ret = 0;
    while (std::isxdigit(*r))
      ret = ret * 16 + hex2int(*r++);
    return ret;
  }

private:
  std::ostream& put_colrow(std::ostream& ostr, unsigned char a) {
    return ostr << unsigned(a / 16) << "/" << unsigned(a % 16);
  }
  void dump_charset(sb94data* charset) {
    std::cout << "ISO-IR-" << charset->reg << ": "
              << charset->name << "\n";

    const char* type_name = "unknown_type";
    switch (charset->type) {
    case charset_sb94: type_name = "SB94"; break;
    }
    std::cout << "  Type: " << type_name << "\n";

    std::cout << "  Escape sequence: ";
    for (char a : charset->seq)
      put_colrow(std::cout, a) << " ";
    std::cout << "(" << charset->seq << ")\n";
    for (char a = '!';  a <= '~'; a++) {
      char32_t b = charset->map[(int) a];
      std::cout << "  " << a << " (";
      put_colrow(std::cout, a);
      std::cout << "): ";
      if (b == unused) {
        std::cout << "unused";
      } else {
        contra::encoding::put_u8(b, std::cout);
        std::cout << " (U+" << std::hex << (std::uint32_t) b << ")";
      }
      if (b != (char32_t) a) std::cout << " *";
      std::cout << "\n";
    }
    std::cout << std::endl;
  }

private:
  bool process_command_sb94() {
    sb94data data;
    data.type = charset_sb94;

    // seq
    if (!read_until(')', data.seq)) {
      print_error() << "no corresponding \")\" found" << std::endl;
      return false;
    }
    r++;

    // reg
    skip_space();
    if (!starts_with_skip("ISO-IR-")) {
      print_error() << "no ISO-IR registration number" << std::endl;
      return false;
    }
    if (!read_until(' ', data.reg)) {
      print_error() << "unrecognized ISO-IR registration number" << std::endl;
      return false;
    }

    // name
    skip_space();
    read_until('\0', data.name);

    auto& record = charset94[data.seq];
    record = std::move(data);
    if (current_charset_data)
      dump_charset(current_charset_data);
    current_charset_data = &record;
    return true;
  }

  bool process_command_map() {
    if (!current_charset_data) {
      print_error() << "charset unselected" << std::endl;
      return false;
    }

    while (*r) {
      char32_t a = *r++;
      if (!(0x21 <= a && a <= 0x7E)) {
        print_error() << "invalid code" << std::endl;
        return false;
      }

      char32_t b;
      if (!*r) {
        print_error() << "unexpected char" << std::endl;
        return false;
      } else if (starts_with_skip("<U+")) {
        b = read_hex();
        if (!starts_with_skip(">")) {
          print_error() << "unexpected unicode spec <U+XXXX>" << std::endl;
          return false;
        }
      } else if (starts_with_skip("<undef>")) {
        b = unused;
      } else {
        b = *r++;
      }
      current_charset_data->map[(int) a] = b;
    }
    return true;
  }
  bool process_command_load() {
    if (!current_charset_data) {
      print_error() << "charset unselected" << std::endl;
      return false;
    }

    if (starts_with_skip("SB94(")) {
      std::string seq;
      if (!read_until(')', seq)) {
        print_error() << "unrecognized charset spec" << std::endl;
        return false;
      }
      r++;

      auto it = charset94.find(seq);
      if (it == charset94.end()) {
        print_error() << "SB94(" << seq << ") is not defined" << std::endl;
        return false;
      }

      for (char a = '!';  a <= '~'; a++) {
        char32_t const b = it->second.map[(int) a];
        if (b != unused)
          current_charset_data->map[(int) a] = b;
      }
      return true;
    } else {
      print_error() << "unrecognized charset spec" << std::endl;
      return false;
    }
  }
  bool process_command_undef() {
    if (!current_charset_data) {
      print_error() << "charset unselected" << std::endl;
      return false;
    }

    while (*r) {
      char32_t a = *r++;
      if (!(0x21 <= a && a <= 0x7E)) {
        print_error() << "invalid code" << std::endl;
        return false;
      }
      current_charset_data->map[(int) a] = unused;
    }
    return true;
  }

private:
  bool process_line() {
    skip_space();
    if (!*r || *r == '#') {
      return true;
    } else if (starts_with_skip("SB94(")) {
      return process_command_sb94();
    } else if (starts_with_skip("load ")) {
      return process_command_load();
    } else if (starts_with_skip("map ")) {
      return process_command_map();
    } else if (starts_with_skip("undef ")) {
      return process_command_undef();
    } else {
      print_error() << "unrecognized command" << std::endl;
      return false;
    }
  }

public:
  void process(std::istream& istr) {
    iline = 0;
    std::string line;
    std::vector<char32_t> buffer;
    while (std::getline(istr, line)) {
      iline++;
      char const* p = &line[0];
      char const* pN = p + line.size();
      buffer.resize(line.size() + 1);
      char32_t* const q0 = &buffer[0];
      char32_t* const qN = q0 + line.size();
      char32_t* q1 = &buffer[0];
      std::uint64_t state = 0;
      contra::encoding::utf8_decode(p, pN, q1, qN, state);
      if (state) *q1++ = 0xFFFD;
      *q1++ = U'\0';

      r = r0 = q0;
      rN = q1;
      process_line();
    }
    if (current_charset_data)
      dump_charset(current_charset_data);
  }
};

int main() {
  std::ifstream ifs("iso2022.txt");
  processor_t proc;
  proc.process(ifs);
  return 0;
}
