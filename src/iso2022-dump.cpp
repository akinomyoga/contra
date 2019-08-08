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
char32_t invalid_code = 0xFFFFFFFE;

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

  void print_html_header(std::ostream& ostr) {
    ostr << "<?DOCTYPE html>\n"
         << "<html>\n"
         << "<head>\n"
         << "<style>\n"
         << "table.contra-iso2022-table { border-collapse: collapse; margin: auto; }\n"
         << "table.contra-iso2022-table td { border: 1px solid gray; text-align: center; font-size: 1.5em; height: 2em; width: 2em; }\n"
         << "table.contra-iso2022-table td>code { font-size: 0.7rem; }\n"
         << "td.contra-iso2022-undef { background-color: #ddd; }\n"
         << "td.contra-iso2022-diff { background-color: #dfd; }\n"
         << "</style>\n"
         << "</head>\n"
         << "<body>\n";
  }
  void print_html_footer(std::ostream& ostr) {
    ostr << "</body>\n"
         << "</html>\n";
  }
  void print_html_charset(std::ostream& ostr, sb94data* charset) {
    ostr << "<h2>ISO-IR-" << charset->reg << ": "
         << charset->name << "</h2>\n";

    ostr << "<ul>\n";
    {
      const char* type_name = "unknown_type";
      switch (charset->type) {
      case charset_sb94: type_name = "SB94"; break;
      }
      ostr << "<li>Type: " << type_name << "</li>\n";
    }
    {
      ostr << "<li>Escape sequence: ";
      for (char a : charset->seq) {
        put_colrow(ostr, a) << " ";
        if ('@' <= a && a <= '~') break;
      }
      ostr << "(" << charset->seq << ")</li>\n";
    }
    {
      std::string url = "https://www.itscj.ipsj.or.jp/iso-ir/";
      for (int i = strspn(charset->reg.c_str(), "0123456789"); i < 3; i++)
        url += '0';
      url = url + charset->reg + ".pdf";
      ostr << "<li>PDF: <a href=\"" << url << "\">" << url << "</a></li>\n";
    }
    ostr << "</ul>\n";

    ostr << "<table class=\"contra-iso2022-table\">\n";
    {
      ostr << "<tr>";
      ostr << "<th></th>";
      for (int row = 0; row <= 15; row++)
        ostr << "<th>*/" << row << "</th>";
      ostr << "</tr>\n";
    }
    for (int col = 2; col <= 7; col++) {
      ostr << "<tr>";
      ostr << "<th>" << col << "/*</th>";
      for (int row = 0; row <= 15; row++) {
        char const a = col << 4 | row;
        char32_t b = charset->map[(int) a];
        if (b == unused) {
          ostr << "<td class=\"contra-iso2022-undef\"></td>";
        } else {
          if (b != (char32_t) a)
            ostr << "<td class=\"contra-iso2022-diff\">";
          else
            ostr << "<td>";

          if ((0x300<= b && b < 0x370) || (0x20D0 <= b && b < 0x2100) || (0xFE20 <= b && b < 0xFE30)) ostr << "&#x25cc;";
          contra::encoding::put_u8(b, ostr);
          std::ios_base::fmtflags old_flags = std::cout.flags();
          ostr << "<br/><code>U+"
               << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
               << (std::uint32_t) b << "</code>";
          ostr << "</td>";
          ostr.flags(old_flags);
        }
      }
      ostr << "</tr>\n";
    }
    ostr << "</table>\n";
    ostr << std::endl;
  }
  void dump_charset(sb94data* charset) {
    print_html_charset(ostr_html, charset);

    std::cout << "ISO-IR-" << charset->reg << ": "
              << charset->name << "\n";

    const char* type_name = "unknown_type";
    switch (charset->type) {
    case charset_sb94: type_name = "SB94"; break;
    }
    std::cout << "  Type: " << type_name << "\n";

    std::cout << "  Escape sequence: ";
    for (char a : charset->seq) {
      put_colrow(std::cout, a) << " ";
      if ('@' <= a && a <= '~') break;
    }
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
        std::ios_base::fmtflags old_flags = std::cout.flags();
        std::cout << " (U+"
                  << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
                  << (std::uint32_t) b << ")";
        std::cout.flags(old_flags);
      }
      if (b != (char32_t) a) std::cout << " *";
      std::cout << "\n";
    }
    std::cout << std::endl;
  }

private:
  char32_t read_code() {
    char32_t const a = *r++;
    if (!a) {
      print_error() << "missing start of range" << std::endl;
      return invalid_code;
    } else if (!(0x21 <= a && a <= 0x7E)) {
      print_error() << "invalid code" << std::endl;
      return invalid_code;
    }
    return a;
  }
  char32_t read_unicode() {
    char32_t b;
    if (!*r) {
      print_error() << "unexpected char" << std::endl;
      return invalid_code;
    } else if (starts_with_skip("<U+")) {
      b = read_hex();
      if (!starts_with_skip(">")) {
        print_error() << "invalid form of unicode spec <U+XXXX>" << std::endl;
        return invalid_code;
      }
    } else if (starts_with_skip("<undef>")) {
      b = unused;
    } else {
      b = *r++;
    }
    return b;
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
    if (current_charset_data)
      dump_charset(current_charset_data);
    record = std::move(data);
    current_charset_data = &record;
    return true;
  }

  bool process_command_map_range() {
    if (!current_charset_data) {
      print_error() << "charset unselected" << std::endl;
      return false;
    }

    skip_space();
    char32_t a = read_code();
    if (a == invalid_code) return false;
    char32_t b = read_unicode();
    if (b == invalid_code) return false;

    if (!starts_with_skip("-")) {
      print_error() << "'-' is expected" << std::endl;
      return false;
    }

    char32_t const c = read_code();
    if (c == invalid_code) return false;
    char32_t const d = read_unicode();
    if (d == invalid_code) return false;

    if (!(a < c && b < d)) {
      print_error() << "range beg/end are inverted" << std::endl;
      return false;
    } else if (c - a != d - b) {
      print_error() << "lengths of ranges do not match" << std::endl;
      return false;
    }

    for (; a <= c; a++, b++)
      current_charset_data->map[(int) a] = b;
    return true;
  }
  bool process_command_undef_range() {
    if (!current_charset_data) {
      print_error() << "charset unselected" << std::endl;
      return false;
    }

    skip_space();
    char32_t a = read_code();
    if (a == invalid_code) return false;

    if (!starts_with_skip("-")) {
      print_error() << "'-' is expected" << std::endl;
      return false;
    }

    char32_t const c = read_code();
    if (c == invalid_code) return false;

    if (!(a < c)) {
      print_error() << "range beg/end are inverted" << std::endl;
      return false;
    }

    for (; a <= c; a++)
      current_charset_data->map[(int) a] = unused;
    return true;
  }
  bool process_command_map(bool check = false) {
    if (!current_charset_data) {
      print_error() << "charset unselected" << std::endl;
      return false;
    }

    while (*r) {
      char32_t const a = read_code();
      if (a == invalid_code) return false;

      char32_t const b = read_unicode();
      if (b == invalid_code) return false;

      if (check) {
        if (current_charset_data->map[(int) a] != b) {
          put_colrow(print_error() << "check_map: ", a) << " does not match" << std::endl;
        }
      } else
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
    } else if (starts_with_skip("map_check ")) {
      return process_command_map(true);
    } else if (starts_with_skip("map_range ")) {
      return process_command_map_range();
    } else if (starts_with_skip("undef ")) {
      return process_command_undef();
    } else if (starts_with_skip("undef_range ")) {
      return process_command_undef_range();
    } else {
      print_error() << "unrecognized command" << std::endl;
      return false;
    }
  }

private:
  std::ofstream ostr_html;

public:
  int process(std::istream& istr) {
    ostr_html.open("iso2022.html");
    print_html_header(ostr_html);

    iline = 0;
    bool error_flag = false;
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
      if (!process_line())
        error_flag = true;
    }
    if (current_charset_data)
      dump_charset(current_charset_data);

    print_html_footer(ostr_html);
    ostr_html.close();

    if (error_flag) return 1;
    return 0;
  }
};

int main() {
  std::ifstream ifs("iso2022.def");
  if (!ifs) {
    std::cerr << "failed to open the fil iso2022.def" << std::endl;
    return 1;
  }
  processor_t proc;
  return proc.process(ifs);
}
