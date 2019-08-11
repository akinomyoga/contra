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
#include <mwg/except.h>
#include "enc.utf8.hpp"
#include "iso2022.hpp"

static std::ostream& put_colrow(std::ostream& ostr, unsigned char a) {
  return ostr << unsigned(a / 16) << "/" << unsigned(a % 16);
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

static void put_for_html(char32_t u, std::ostream& ostr) {
  switch (u) {
  case U'<': ostr << "&lt;"; break;
  case U'>': ostr << "&gt;"; break;
  case U'&': ostr << "&amp;"; break;
  default:
    contra::encoding::put_u8(u, ostr);
  }
}

typedef unsigned char byte;

using namespace contra;

class iso2022_definition_dumper {
  std::ofstream ostr_html;
  std::ofstream ostr_text;
  std::ofstream ostr_cpp;

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
         << "<h1>94/96-Character Graphic Character Sets</h1>\n";
  }
  void print_html_footer(std::ostream& ostr) {
    ostr << "</body>\n"
         << "</html>\n";
  }
  void print_html_charset(std::ostream& ostr, iso2022_charset const* charset) {
    ostr << "<h2>ISO-IR-" << charset->reg << ": "
         << charset->name << "</h2>\n";

    ostr << "<ul>\n";
    {
      const char* type_name = "unknown_type";
      switch (charset->type) {
      case iso2022_size_sb94:
      case iso2022_size_mb94:
        type_name = "94";
        break;
      case iso2022_size_sb96:
      case iso2022_size_mb96:
        type_name = "96";
        break;
      }
      ostr << "<li>Type: " << type_name;
      if (charset->number_of_bytes !=  1)
        ostr << "<sup>" << charset->number_of_bytes << "</sup>";
      ostr << "</li>\n";
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

    // 違いを比較する対象の文字コードのずれ
    unsigned char meta_shift = 0;
    if (charset->type == iso2022_size_sb96)
      meta_shift = 0x80;

    ostr << "<table class=\"contra-iso2022-table\">\n";
    {
      ostr << "<tr>";
      ostr << "<th></th>";
      for (int row = 0; row <= 15; row++)
        ostr << "<th>*/" << row << "</th>";
      ostr << "</tr>\n";
    }
    std::vector<char32_t> vec;
    for (int col = 2; col <= 7; col++) {
      ostr << "<tr>";
      ostr << "<th>" << col << "/*</th>";
      for (int row = 0; row <= 15; row++) {
        char const a = col << 4 | row;

        if (!charset->get_chars(vec, charset->kuten2index(a))) {
          ostr << "<td class=\"contra-iso2022-undef\"></td>";
        } else {
          if (vec.size() != 1 || vec[0] != (char32_t) a + meta_shift)
            ostr << "<td class=\"contra-iso2022-diff\">";
          else
            ostr << "<td>";

          if (is_combining_character(vec[0])) ostr << "&#x25cc;";
          for (auto const b : vec) put_for_html(b, ostr);

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
        }
      }
      ostr << "</tr>\n";
    }
    ostr << "</table>\n";
    ostr << std::endl;
  }
  void print_text(std::ostream& ostr, iso2022_charset const* charset) {
    ostr << "ISO-IR-" << charset->reg << ": "
              << charset->name << "\n";

    unsigned char min1, max1;
    unsigned char meta_shift = 0;
    const char* type_name = "unknown_type";
    switch (charset->type) {
    default:
    case iso2022_size_sb94:
      type_name = "SB94";
      min1 = '!'; max1 = '~';
      break;
    case iso2022_size_sb96:
      type_name = "SB96";
      min1 = 0x20; max1 = 0x7F;
      meta_shift = 0x80;
      break;
    }
    ostr << "  Type: " << type_name << "\n";

    ostr << "  Escape sequence: ";
    for (char a : charset->seq) {
      put_colrow(ostr, a) << " ";
      if ('@' <= a && a <= '~') break;
    }
    ostr << "(" << charset->seq << ")\n";
    std::vector<char32_t> vec;
    for (unsigned char a = min1; a <= max1; a++) {
      ostr << "  " << a << " (";
      put_colrow(ostr, a);
      ostr << "): ";

      if (!charset->get_chars(vec, charset->kuten2index(a))) {
        ostr << "undefined";
      } else {
        if (is_combining_character(vec[0])) ostr << "&#x25cc;";
        for (auto const b : vec) contra::encoding::put_u8(b, ostr);

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

        if (vec.size() != 1 || vec[0] != (char32_t) a + meta_shift) ostr << " *";
      }

      ostr << "\n";
    }
    ostr << std::endl;
  }

  void print_cpp(std::ostream& ostr, iso2022_charset const* charset) {
    ostr << "// ISO-IR-" << charset->reg << ": "
              << charset->name << "\n";

    const char* type_name = "unknown_type";
    switch (charset->type) {
    default:
    case iso2022_size_sb94:
      type_name = "SB94";
      break;
    case iso2022_size_sb96:
      type_name = "SB96";
      break;
    }
    ostr << "//   Type: " << type_name << "\n";

    ostr << "//   Escape sequence: ";
    for (char a : charset->seq) {
      put_colrow(ostr, a) << " ";
      if ('@' <= a && a <= '~') break;
    }
    ostr << "(" << charset->seq << ")\n";

    ostr << "char32_t const iso_ir_";
    for (char a : charset->reg)
      ostr << (std::isalnum(a)? a : '_');
    ostr << "_table[96] = {\n";

    bool has_multichar = false;
    for (unsigned char a = 0x20; a <= 0x7F; a++) {
      if (a % 8 == 0) ostr << "  ";

      char32_t b = charset->kuten2u(a);
      if (b == undefined_code) {
        ostr << "0x0000,";
      } else if (b == multichar_conv) {
        ostr << "0xFFFF,"; // multichar
        has_multichar = true;
      } else {
        std::ios_base::fmtflags old_flags = std::cout.flags();
        ostr << "0x"
             << std::hex << std::uppercase
             << std::setw(4) << std::setfill('0') << (std::uint32_t) b
             << ",";
        ostr.flags(old_flags);
      }

      ostr << ((a + 1) % 8 ? " " : "\n");
    }
    ostr << "};\n";
    if (has_multichar)
      ostr << "  // Note: 0xFFFF means a sequence of unicode code points\n";
    ostr << std::endl;
  }

  static void on_charset_defined(iso2022_charset const* charset, std::uintptr_t param) {
    reinterpret_cast<iso2022_definition_dumper*>(param)->dump_charset(charset);
  }
public:
  bool process(iso2022_charset_registry& iso2022, const char* filename) {
    ostr_html.open("out/iso2022.html");
    print_html_header(ostr_html);
    ostr_text.open("out/iso2022.txt");
    ostr_cpp.open("out/iso2022.cpp");

    bool const ret = iso2022.load_definition(filename, &on_charset_defined, reinterpret_cast<std::uintptr_t>(this));

    print_html_footer(ostr_html);
    ostr_html.close();
    ostr_text.close();
    ostr_cpp.close();
    std::cout << output_charset_count << " charsets are processed." << std::endl;
    return ret;
  }

private:
  int output_charset_count = 0;

  void dump_charset(iso2022_charset const* charset) {
    output_charset_count++;
    print_html_charset(ostr_html, charset);
    print_text(ostr_text, charset);
    print_cpp(ostr_cpp, charset);
  }
};

int main() {
  iso2022_charset_registry iso2022;
  iso2022_definition_dumper proc;
  return !proc.process(iso2022, "iso2022.def");
}
