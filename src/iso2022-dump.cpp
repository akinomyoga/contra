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
#include "enc.c2w.hpp"
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
    if (u <= contra::unicode_max)
      contra::encoding::put_u8(u, ostr);
    else
      contra::encoding::put_u8(0xFFFD, ostr);
  }
}

static bool starts_with(std::string const& str, const char* prefix) {
  const char* s = str.c_str();
  while (*prefix)
    if (*s++ != *prefix++)
      return false;
  return true;
}

typedef unsigned char byte;

using namespace contra;

class iso2022_definition_dumper {

  bool is_undefined_ku(iso2022_charset const* charset, int ku_index) {
    for (int a = 32; a < 128; a++) {
      char32_t const kuten = (ku_index + 32) << 8 | a;
      if (charset->kuten2u(kuten) != undefined_code) return false;
    }
    return true;
  }

private:
  void print_html_header(std::ostream& ostr) {
    ostr << "<?DOCTYPE html>\n"
         << "<html>\n"
         << "<head>\n"
         << "<style>\n"
         << "table.contra-iso2022-table { border-collapse: collapse; margin: auto; }\n"
         << "table.contra-iso2022-table td { border: 1px solid gray; text-align: center; font-size: 1.5em; height: 2em; width: 2em; position: relative; }\n"
         << "table.contra-iso2022-table td>code { font-size: 0.7rem; }\n"
         << "table.contra-iso2022-table td>span.c2w { font-size: 0.7rem; position: absolute; left: 0; top: 0; }\n"
         << "td.contra-iso2022-undef { background-color: #ddd; }\n"
         << "td.contra-iso2022-diff { background-color: #dfd; }\n"
         << "td.contra-iso2022-narrow { background-color: #fdd; }\n"
         << "th.contra-iso2022-ku { font-size: 1.5em; padding-right: 0.5em; }\n"
         << "</style>\n"
         << "</head>\n"
         << "<body>\n";
  }
  void print_html_footer(std::ostream& ostr) {
    ostr << "</body>\n"
         << "</html>\n";
  }
  /// @param[in] meta_shift 違いを比較する対象の文字コードのずれ
  void print_html_ku(std::ostream& ostr, iso2022_charset const* charset, int ku_index, unsigned char meta_shift = 0) {
    bool multibyte = ku_index >= 0;
    if (multibyte && is_undefined_ku(charset, ku_index)) return;

    ostr << "<table class=\"contra-iso2022-table\">\n";
    {
      ostr << "<tr>";
      if (multibyte)
        ostr << "<th class=\"contra-iso2022-ku\" rowspan=\"7\">" << ku_index << "区</th>";
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
        char32_t const a = (multibyte ? ku_index + 32 : 0) << 8 | col << 4 | row;

        if (!charset->get_chars(vec, charset->kuten2index(a))) {
          ostr << "<td class=\"contra-iso2022-undef\"></td>";
        } else {
          if (multibyte) {
            if (contra::encoding::c2w(vec[0], contra::encoding::c2w_width_east) == 1)
              ostr << "<td class=\"contra-iso2022-narrow\">";
            else
              ostr << "<td>";

            if (contra::encoding::is_ambiguous(vec[0]))
              ostr << "<span class=\"c2w\" title=\"ambiguous\">A</span>";
          } else {
            if (vec.size() != 1 || vec[0] != a + meta_shift)
              ostr << "<td class=\"contra-iso2022-diff\">";
            else
              ostr << "<td>";
          }


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
  }
  void print_html_charset(std::ostream& ostr, iso2022_charset const* charset) {
    ostr << "<h2>" << charset->reg << ": "
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
    if (starts_with(charset->reg, "ISO-IR-")) {
      const char* ir_number = charset->reg.c_str() + 7;
      std::string url = "https://www.itscj.ipsj.or.jp/iso-ir/";
      for (int i = strspn(ir_number, "0123456789"); i < 3; i++)
        url += '0';
      url = url + ir_number + ".pdf";
      ostr << "<li>Definition: <a href=\"" << url << "\">" << url << "</a></li>\n";
    }
    ostr << "</ul>\n";

    switch (charset->type) {
    case iso2022_size_sb96:
      print_html_ku(ostr, charset, -1, 0x80);
      break;
    case iso2022_size_sb94:
      print_html_ku(ostr, charset, -1);
      break;
    case iso2022_size_mb94:
      for (int k = 1; k <= 94; k++) print_html_ku(ostr, charset, k);
      break;
    case iso2022_size_mb96:
      for (int k = 0; k <= 95; k++) print_html_ku(ostr, charset, k);
      break;
    }

    ostr << std::endl;
  }

private:
  void print_text(std::ostream& ostr, iso2022_charset const* charset) {
    ostr << charset->reg << ": " << charset->name << "\n";

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
    ostr << "// " << charset->reg << ": "
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

    ostr << "char32_t const ";
    for (char a : charset->reg)
      ostr << (char) (std::isalnum(a) ? std::tolower(a) : '_');
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
        std::ios_base::fmtflags old_flags = ostr.flags();
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

  iso2022_charset_registry* m_iso2022 = nullptr;
  std::vector<charset_t> m_charset_list;

  static void on_charset_defined(iso2022_charset const* charset, charset_t csindex, std::uintptr_t param) {
    contra_unused(charset);
    reinterpret_cast<iso2022_definition_dumper*>(param)->m_charset_list.push_back(csindex);
  }
public:
  bool process(iso2022_charset_registry& iso2022, const char* filename) {
    m_iso2022 = &iso2022;
    bool const ret = iso2022.load_definition(filename, &on_charset_defined, reinterpret_cast<std::uintptr_t>(this));
    std::cout << m_charset_list.size() << " charsets are defined." << std::endl;
    return ret;
  }

public:
  void save_html(std::string const& filename, std::string const& title) {
    std::ofstream ostr(filename);
    print_html_header(ostr);
    if (!title.empty()) ostr << "<h1>" << title << "</h1>\n";
    for (auto index : m_charset_list)
      print_html_charset(ostr, m_iso2022->charset(index));
    print_html_footer(ostr);
  }
  void save_cpp(std::string const& filename) {
    std::ofstream ostr(filename);
    for (auto index : m_charset_list)
      print_cpp(ostr, m_iso2022->charset(index));
  }
  void save_txt(std::string const& filename) {
    std::ofstream ostr(filename);
    for (auto index : m_charset_list)
      print_text(ostr, m_iso2022->charset(index));
  }

  void save_html_separate(std::string const& dirname) {
    for (auto index : m_charset_list) {
      auto const charset = m_iso2022->charset(index);
      std::string fname_html = dirname + "/";
      for (char a : charset->reg)
        fname_html += (char) (std::isalnum(a) ? std::tolower(a) : '_');
      fname_html += ".html";
      std::ofstream file(fname_html);
      if (!file){
        std::cerr << "failed to open the file '" << fname_html << "'" << std::endl;
        return;
      }
      print_html_header(file);
      print_html_charset(file, charset);
      print_html_footer(file);
    }
  }
};

bool load_iso2022_def() {
  iso2022_charset_registry iso2022;
  iso2022_definition_dumper proc;
  if (!proc.process(iso2022, "res/iso2022.def")) return false;
  proc.save_html("out/iso2022.html", "94/96-Character Graphic Character Sets");
  // proc.save_cpp("out/iso2022.cpp");
  // proc.save_txt("out/iso2022.txt");
  return true;
}
bool load_jis_def() {
  iso2022_charset_registry iso2022;
  iso2022_definition_dumper proc;
  if (!proc.process(iso2022, "res/iso2022-jis.def")) return false;
  proc.save_html_separate("out");
  return true;
}

bool load_file(const char* filename) {
  iso2022_charset_registry iso2022;
  iso2022_definition_dumper proc;
  if (!proc.process(iso2022, filename)) return false;
  return true;
}

int main(int argc, char** argv) {
  if (argc == 1) {
    if (!load_iso2022_def()) return 1;
    if (!load_jis_def()) return 1;
  } else {
    if (!load_file(argv[1])) return 1;
  }
  return 0;
}
