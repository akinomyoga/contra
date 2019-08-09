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

static constexpr char32_t undefined_code = 0xFFFFFFFF;
static constexpr char32_t invalid_code = 0xFFFFFFFE;
static constexpr char32_t multichar_conv = 0xFFFFFFFD;

enum charset_type {
  charset_sb94,
  charset_sb96,
  charset_mb94,
  charset_mb96,
};

struct sb_charset_data {
  int type;
  std::string seq;
  std::string reg;
  std::string name;
  char32_t map[128];
  std::vector<char32_t> multimap[128];

  sb_charset_data() {
    std::fill(std::begin(map), std::end(map), undefined_code);
  }
};

struct processor_t {
  typedef std::unordered_map<std::string, sb_charset_data> charset_map_type;
  std::unordered_map<std::string, sb_charset_data> charset94;
  std::unordered_map<std::string, sb_charset_data> charset96;

  int iline = 0;
  char32_t const* r;
  char32_t const* r0;
  char32_t const* rN;

  sb_charset_data* current_charset_data = nullptr;

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

  bool is_graphic_character(char32_t u) const {
    if (!current_charset_data) return false;
    switch (current_charset_data->type) {
    case charset_sb94:
      return 0x21 <= u && u <= 0x7E;
    case charset_sb96:
      return 0x20 <= u && u <= 0x80;
    default:
      return false;
    }
  }
  static constexpr bool is_combining_character(char32_t u) {
    if (0x300 <= u && u < 0x370) return true;
    if (0x610 <= u && u <= 0x61a) return true;
    if ((0x64B <= u && u <= 0x65F) || u == 0x670) return true;
    if ((0x6D6 <= u && u <= 0x6ED) &&
      (u != 0x6DD && u != 0x6DE && u != 0x6E5 && u != 0x6E6 && u != 0x6E9))
      return true;
    if (0x20D0 <= u && u < 0x2100) return true;
    if (0xFE20 <= u && u < 0xFE30) return true;
    return false;
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
         << "<body>\n"
         << "<h1>94-Character Graphic Character Sets</h1>\n";
  }
  void print_html_footer(std::ostream& ostr) {
    ostr << "</body>\n"
         << "</html>\n";
  }
  void print_html_charset(std::ostream& ostr, sb_charset_data* charset) {
    ostr << "<h2>ISO-IR-" << charset->reg << ": "
         << charset->name << "</h2>\n";

    ostr << "<ul>\n";
    {
      const char* type_name = "unknown_type";
      switch (charset->type) {
      case charset_sb94: type_name = "SB94"; break;
      case charset_sb96: type_name = "SB96"; break;
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
        if (b == undefined_code) {
          ostr << "<td class=\"contra-iso2022-undef\"></td>";
        } else if (b == multichar_conv) {
          auto const& vec = charset->multimap[(int) a];
          if (vec.size() < 2) {
            ostr << "<td>ERROR</td>";
            std::cerr << "ISO-IR-" << charset->reg << ": BUG? invalid multichar_conv" <<std::endl;
            continue;
          }

          ostr << "<td class=\"contra-iso2022-diff\">";
          if (is_combining_character(vec[0])) ostr << "&#x25cc;";
          for (auto const b : vec)
            contra::encoding::put_u8(b, ostr);

          std::ios_base::fmtflags old_flags = std::cout.flags();
          {
            ostr << "<br/><code>";
            bool first = true;
            for (auto const b : vec) {
              if (first) first = false; else ostr << " ";
              ostr << "U+"
                   << std::hex << std::uppercase
                   << std::setw(4) << std::setfill('0') << (std::uint32_t) b;
            }
            ostr << "</code></td>";
          }
          ostr.flags(old_flags);
        } else {
          if (b != (char32_t) a)
            ostr << "<td class=\"contra-iso2022-diff\">";
          else
            ostr << "<td>";

          if (is_combining_character(b)) ostr << "&#x25cc;";
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
  void print_text(std::ostream& ostr, sb_charset_data* charset) {
    ostr << "ISO-IR-" << charset->reg << ": "
              << charset->name << "\n";

    const char* type_name = "unknown_type";
    switch (charset->type) {
    case charset_sb94: type_name = "SB94"; break;
    case charset_sb96: type_name = "SB96"; break;
    }
    ostr << "  Type: " << type_name << "\n";

    ostr << "  Escape sequence: ";
    for (char a : charset->seq) {
      put_colrow(ostr, a) << " ";
      if ('@' <= a && a <= '~') break;
    }
    ostr << "(" << charset->seq << ")\n";
    for (char a = '!';  a <= '~'; a++) {
      char32_t b = charset->map[(int) a];
      ostr << "  " << a << " (";
      put_colrow(ostr, a);
      ostr << "): ";
      if (b == undefined_code) {
        ostr << "undefined";
      } else if (b == multichar_conv) {
        auto const& vec = charset->multimap[(int) a];
        if (vec.size() < 2) {
          ostr << "ERROR";
          std::cerr << "ISO-IR-" << charset->reg << ": BUG? invalid multichar_conv" <<std::endl;
          continue;
        }

        if (is_combining_character(vec[0])) ostr << "&#x25cc;";
        for (auto const b : vec)
          contra::encoding::put_u8(b, ostr);

        std::ios_base::fmtflags old_flags = std::cout.flags();
        {
          ostr << " (";
          bool first = true;
          for (auto const b : vec) {
            if (first) first = false; else ostr << " ";
            ostr << "U+"
                 << std::hex << std::uppercase
                 << std::setw(4) << std::setfill('0') << (std::uint32_t) b;
          }
          ostr << ")";
        }
        ostr.flags(old_flags);
      } else {
        contra::encoding::put_u8(b, ostr);
        ostr << " (U+"
             << std::hex << std::uppercase
             << std::setw(4) << std::setfill('0') << (std::uint32_t) b
             << ")";
      }
      if (b != (char32_t) a) ostr << " *";
      ostr << "\n";
    }
    ostr << std::endl;
  }
  void dump_charset(sb_charset_data* charset) {
    print_html_charset(ostr_html, charset);
    print_text(std::cout, charset);
  }

private:
  char32_t read_code() {
    char32_t const a = *r;
    if (!a) {
      return invalid_code;
    } else if (!is_graphic_character(a)) {
      print_error() << "invalid code" << std::endl;
      return invalid_code;
    }
    r++;
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
      b = undefined_code;
    } else {
      b = *r++;
    }
    return b;
  }

private:
  bool process_command_declare(charset_type type) {
    sb_charset_data data;
    data.type = type;

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

    auto& record = type == charset_sb96 ? charset96[data.seq] : charset94[data.seq];
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
    if (a == invalid_code) {
      print_error() << "missing start of range" << std::endl;
      return false;
    }
    if (a == invalid_code) return false;
    char32_t b = read_unicode();
    if (b == invalid_code) return false;

    if (!starts_with_skip("-")) {
      print_error() << "'-' is expected" << std::endl;
      return false;
    }

    char32_t const c = read_code();
    if (c == invalid_code) {
      print_error() << "missing end of range" << std::endl;
      return false;
    }
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
      current_charset_data->map[(int) a] = undefined_code;
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
  bool load_charset_impl(const char* name, charset_map_type const& charset94) {
    std::string seq;
    if (!read_until(')', seq)) {
      print_error() << "unrecognized charset spec" << std::endl;
      return false;
    }
    r++;

    auto it = charset94.find(seq);
    if (it == charset94.end()) {
      print_error() << name << "(" << seq << ") is not defined" << std::endl;
      return false;
    }

    for (char a = '!';  a <= '~'; a++) {
      char32_t const b = it->second.map[(int) a];
      if (b != undefined_code) {
        current_charset_data->map[(int) a] = b;
        if (b == multichar_conv)
          current_charset_data->multimap[(int) a]
            = it->second.multimap[(int) a];
      }
    }
    return true;
  }
  bool process_command_load() {
    if (!current_charset_data) {
      print_error() << "charset unselected" << std::endl;
      return false;
    }

    if (starts_with_skip("SB94(")) {
      return load_charset_impl("SB94", charset94);
    } else if (starts_with_skip("SB96(")) {
      return load_charset_impl("SB96", charset96);
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
      char32_t a = read_code();
      if (a == invalid_code) return false;
      current_charset_data->map[(int) a] = undefined_code;
    }
    return true;
  }
  bool process_command_define() {
    skip_space();
    if (!*r) {
      print_error() << "no code specified" << std::endl;
      return false;
    }

    char32_t a = read_code();
    if (a == invalid_code) return false;

    auto& vec = current_charset_data->multimap[(int) a];
    vec.clear();
    while (*r) {
      char32_t const b = read_unicode();
      if (b == invalid_code) return false;
      vec.push_back(b);
    }

    if (vec.size() == 0) {
      print_error() << "no characters are specified" << std::endl;
      return false;
    } else if (vec.size() == 1) {
      current_charset_data->map[(int) a] = vec[0];
    } else {
      current_charset_data->map[(int) a] = multichar_conv;
    }
    return true;
  }

private:
  bool process_line() {
    skip_space();
    if (!*r || *r == '#') {
      return true;
    } else if (starts_with_skip("SB94(")) {
      return process_command_declare(charset_sb94);
    } else if (starts_with_skip("SB96(")) {
      return process_command_declare(charset_sb96);
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
    } else if (starts_with_skip("define ")) {
      return process_command_define();
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
