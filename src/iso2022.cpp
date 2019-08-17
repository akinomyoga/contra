#include "iso2022.hpp"
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include "enc.utf8.hpp"

using namespace contra;

namespace {

  std::ostream& put_colrow(std::ostream& ostr, unsigned char a) {
    return ostr << unsigned(a / 16) << "/" << unsigned(a % 16);
  }

  struct iso2022_definition_reader {
    iso2022_charset_registry& iso2022;
    iso2022_charset_callback callback = nullptr;
    std::uintptr_t cbparam = 0;
    const char* title = "(stream)";

  public:
    iso2022_definition_reader(iso2022_charset_registry& iso2022): iso2022(iso2022) {}

  private:
    int iline = 0;
    char32_t const* r;
    char32_t const* r0;
    char32_t const* rN;

    charset_t current_csindex = 0;
    iso2022_charset* current_charset = nullptr;

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
      return std::cerr << title << ":" << iline << ":" << r - r0 << ": ";
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
      if (!current_charset) return false;
      switch (current_charset->type) {
      case iso2022_size_sb94:
      case iso2022_size_mb94:
        return 0x21 <= u && u <= 0x7E;
      case iso2022_size_sb96:
      case iso2022_size_mb96:
        return 0x20 <= u && u <= 0x80;
      default:
        return false;
      }
    }

  private:
    char32_t read_code() {
      char32_t const a = *r;
      if (!a) {
        return invalid_code;
      } else if (!is_graphic_character(a)) {
        print_error() << "invalid code" << std::endl;
        return invalid_code;
      } else if (starts_with_skip("<SP>")) {
        return U' ';
      } else if (starts_with_skip("<DEL>")) {
        return U'\u007F';
      } else {
        r++;
        return a;
      }
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

    char32_t read_kuten() {
      // 前提: read_kuten は current_charset を確認した後でしか呼び出さない
      mwg_check(current_charset);
      char32_t kuten = 0;
      for (std::size_t i = 0, n = current_charset->number_of_bytes; i < n; i++) {
        char32_t const a = read_code();
        if (a == invalid_code) return invalid_code;
        kuten = kuten << 8 | (byte) a;
      }
      return kuten;
    }
    char32_t next_kuten(char32_t kuten) {
      char32_t a = kuten & 0xFF;
      if (a == 0) a = 0x20;
      a++;
      if (a < 0x80)
        return (kuten & ~(char32_t) 0xFF) | a;
      else
        return next_kuten(kuten >> 8) << 8 | 0x20;
    }

  private:
    bool process_command_declare(iso2022_charset_size type) {
      int number_of_bytes = 1;
      if (type == iso2022_size_mb94 || type == iso2022_size_mb96) {
        number_of_bytes = read_unsigned();
        if (number_of_bytes < 2) {
          print_error() << "multibyte charset: number of bytes should be >= 2" << std::endl;
          return false;
        } else if (4 < number_of_bytes) {
          print_error() << "multibyte charset: number of bytes larger than 4 not supported" << std::endl;
          return false;
        }
        skip_space();
        if (*r != ',') {
          print_error() << "\",\" is expected here" << std::endl;
          return false;
        }
        r++;
      }

      iso2022_charset data(type, number_of_bytes);

      // seq
      if (!read_until(')', data.seq)) {
        print_error() << "no corresponding \")\" found" << std::endl;
        return false;
      } else if (data.seq.empty()) {
        print_error() << "empty designator is not allowed" << std::endl;
        return false;
      }
      r++;

      // reg
      skip_space();
      if (!read_until(' ', data.reg)) {
        print_error() << "no registration id" << std::endl;
        return false;
      }

      // name
      skip_space();
      read_until('\0', data.name);

      if (current_charset && callback)
        callback(current_charset, current_csindex, cbparam);

      if (iso2022.resolve_designator(data.type, data.seq) != iso2022_unspecified) {
        const char* dbflag = data.type == iso2022_size_mb94 || data.type == iso2022_size_mb96 ? "$" : "";
        char grflag = data.type == iso2022_size_sb96 || data.type == iso2022_size_mb96 ? '-' : '(';
        print_error() << "charset for \"ESC"
                      << dbflag << grflag << data.seq << "\" is redefined" << std::endl;
      }

      charset_t const csindex = iso2022.register_charset(std::move(data), false);
      current_charset = iso2022.charset(csindex);
      current_csindex = csindex;
      return true;
    }

    bool process_command_map_range() {
      if (!current_charset) {
        print_error() << "charset unselected" << std::endl;
        return false;
      }

      skip_space();
      char32_t a = read_kuten();
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

      char32_t const c = read_kuten();
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

      for (; a <= c; a = next_kuten(a), b++)
        current_charset->kuten2u(a) = b;
      return true;
    }
    bool process_command_undef_range() {
      if (!current_charset) {
        print_error() << "charset unselected" << std::endl;
        return false;
      }

      skip_space();
      char32_t a = read_kuten();
      if (a == invalid_code) return false;

      if (!starts_with_skip("-")) {
        print_error() << "'-' is expected" << std::endl;
        return false;
      }

      char32_t const c = read_kuten();
      if (c == invalid_code) return false;

      if (!(a < c)) {
        print_error() << "range beg/end are inverted" << std::endl;
        return false;
      }

      for (; a <= c; a = next_kuten(a))
        current_charset->kuten2u(a) = undefined_code;
      return true;
    }
    bool process_command_map(bool check = false) {
      if (!current_charset) {
        print_error() << "charset unselected" << std::endl;
        return false;
      }

      while (*r) {
        char32_t const a = read_kuten();
        if (a == invalid_code) return false;

        char32_t const b = read_unicode();
        if (b == invalid_code) return false;

        if (check) {
          if (current_charset->kuten2u(a) != b) {
            put_colrow(print_error() << "check_map: ", a) << " does not match" << std::endl;
          }
        } else
          current_charset->kuten2u(a) = b;
      }
      return true;
    }
    bool load_charset_impl(const char* name, iso2022_charset_size const& cssz) {
      std::string seq;
      if (!read_until(')', seq)) {
        print_error() << "unrecognized charset spec" << std::endl;
        return false;
      }
      r++;

      charset_t const cs = iso2022.resolve_designator(cssz, seq);
      if (cs == iso2022_unspecified) {
        print_error() << name << "(" << seq << ") is not defined" << std::endl;
        return false;
      }

      auto const charset1 = iso2022.charset(cs);
      if (charset1->number_of_bytes != current_charset->number_of_bytes) {
        print_error() << name << "(" << seq << ") has different size from current charset" << std::endl;
        return false;
      }

      current_charset->load_other_charset(*charset1);
      return true;
    }
    bool process_command_load() {
      if (!current_charset) {
        print_error() << "charset unselected" << std::endl;
        return false;
      }

      if (starts_with_skip("SB94(")) {
        return load_charset_impl("SB94", iso2022_size_sb94);
      } else if (starts_with_skip("SB96(")) {
        return load_charset_impl("SB96", iso2022_size_sb96);
      } else if (starts_with_skip("MB94(2,")) {
        return load_charset_impl("MB94", iso2022_size_mb94);
      } else if (starts_with_skip("MB96(2,")) {
        return load_charset_impl("MB96", iso2022_size_mb96);
      } else {
        print_error() << "unrecognized charset spec" << std::endl;
        return false;
      }
    }
    bool process_command_undef() {
      if (!current_charset) {
        print_error() << "charset unselected" << std::endl;
        return false;
      }

      while (*r) {
        char32_t a = read_kuten();
        if (a == invalid_code) return false;
        current_charset->kuten2u(a) = undefined_code;
      }
      return true;
    }

    std::vector<char32_t> w_buffer;
    bool process_command_define() {
      skip_space();
      if (!*r) {
        print_error() << "no code specified" << std::endl;
        return false;
      }

      char32_t a = read_kuten();
      if (a == invalid_code) return false;

      auto& vec = this->w_buffer;
      vec.clear();
      while (*r) {
        char32_t const b = read_unicode();
        if (b == invalid_code) return false;
        vec.push_back(b);
      }

      if (vec.size() == 0) {
        print_error() << "no characters are specified" << std::endl;
        return false;
      }

      current_charset->define(current_charset->kuten2index(a), std::move(vec));
      return true;
    }

    bool include_file(std::string const& filename, bool optional = false) {
      std::ifstream file(filename);
      if (!file) {
        if (optional) return true;
        print_error() << "failed to open the include file '" << filename << "'" << std::endl;
        return false;
      }

      const char* old_title = title;
      int old_iline = iline;
      this->title = filename.c_str();
      this->iline = 0;
      bool const ret = process_stream(file);

      this->title = old_title;
      this->iline = old_iline;
      return ret;
    }

    bool process_command_include() {
      skip_space();
      std::string filename;
      read_until('\0', filename);
      if (filename.empty()) {
        print_error() << "no filename specified" << std::endl;
        return false;
      }
      return include_file(filename);
    }

    bool process_command_savebin() {
      if (!current_charset) {
        print_error() << "charset unselected" << std::endl;
        return false;
      }

      skip_space();
      std::string name;
      read_until('\0', name);
      if (name.empty()) {
        print_error() << "no name specified" << std::endl;
        return false;
      }

      std::ofstream ofs1(name + ".bin", std::ios::binary);
      if (!ofs1) {
        print_error() << "failed to open the file '" << name << ".bin'" << std::endl;
        return false;
      }

      std::ofstream ofs2(name + ".def");
      if (!ofs2) {
        print_error() << "failed to open the file '" << name << ".def'" << std::endl;
        return false;
      }

      int ku0, kuZ, ten0, tenZ;
      switch (current_charset->type) {
      case iso2022_size_sb94:
        ku0 = 0; kuZ = 0;
        ten0 = 1; tenZ = 94;
        break;
      case iso2022_size_mb94:
        ku0 = 1; kuZ = 94;
        ten0 = 1; tenZ = 94;
        break;
      case iso2022_size_sb96:
        ku0 = 0; kuZ = 0;
        ten0 = 0; tenZ = 95;
        break;
      case iso2022_size_mb96:
        ku0 = 0; kuZ = 95;
        ten0 = 0; tenZ = 95;
        break;
      default:
        return false;
      }

      iso2022_charset const* charset = current_charset;

      auto _write_utf16le = [&ofs1] (char32_t c) {
                              ofs1.put(0xFF & c);
                              ofs1.put(0xFF & c >> 8);
                            };
      auto _write_enc96 = [&ofs2] (int value) {
                            if (value == 0)
                              ofs2 << "<SP>";
                            else if (value == 95)
                              ofs2 << "<DEL>";
                            else
                              ofs2 << (char) (value + 0x20);
                          };

      std::vector<char32_t> vec;
      for (int ku = ku0; ku <= kuZ; ku++) {
        for (int ten = ten0; ten <= tenZ; ten++) {
          std::uint32_t kuten = (ku + 32) << 8 | (ten + 32);
          char32_t ch = 0;
          if (!charset->get_chars(vec, charset->kuten2index(kuten))) {
            vec.clear();
            ch = 0xFFFD;
          } else if (vec.size() == 1) {
            ch = vec[0];
          } else {
            ch = 0xFFFF; // multichar_conv
            ofs2 << "  define ";
            _write_enc96(ku);
            _write_enc96(ten);
            std::ios_base::fmtflags old_flags = ofs2.flags();
            ofs2 << std::hex << std::uppercase;
            for (char32_t c : vec)
              ofs2 << "<U+" << (std::uint32_t) c << ">";
            ofs2 << "\n";
            ofs2.flags(old_flags);
          }

          if (ch >= 0x10000) {
            ch -= 0x10000;
            _write_utf16le(0xD800 | (ch >> 10 & 0x3FF));
            _write_utf16le(0xDC00 | (ch & 0x3FF));
          } else {
            _write_utf16le(ch);
          }
        }
      }

      return true;
    }

    bool process_command_loadbin() {
      if (!current_charset) {
        print_error() << "charset unselected" << std::endl;
        return false;
      }

      skip_space();
      std::string name;
      read_until('\0', name);
      if (name.empty()) {
        print_error() << "no name specified" << std::endl;
        return false;
      }

      std::ifstream ifs1(name + ".bin", std::ios::binary);
      if (!ifs1) {
        print_error() << "failed to open the file '" << name << ".bin'" << std::endl;
        return false;
      }

      int ku0, kuZ, ten0, tenZ;
      switch (current_charset->type) {
      case iso2022_size_sb94:
        ku0 = 0; kuZ = 0;
        ten0 = 1; tenZ = 94;
        break;
      case iso2022_size_mb94:
        ku0 = 1; kuZ = 94;
        ten0 = 1; tenZ = 94;
        break;
      case iso2022_size_sb96:
        ku0 = 0; kuZ = 0;
        ten0 = 0; tenZ = 95;
        break;
      case iso2022_size_mb96:
        ku0 = 0; kuZ = 95;
        ten0 = 0; tenZ = 95;
        break;
      default:
        return false;
      }

      iso2022_charset* charset = current_charset;

      auto _read_utf16le = [&ifs1] () -> char32_t {
                             unsigned char const a = ifs1.get();
                             unsigned char const b = ifs1.get();
                             return a | b << 8;
                           };

      for (int ku = ku0; ku <= kuZ; ku++) {
        for (int ten = ten0; ten <= tenZ; ten++) {
          std::uint32_t const kuten = (ku + 32) << 8 | (ten + 32);
          char32_t ch = _read_utf16le();
          if (0xD800 <= ch && ch < 0xDC00) {
            // invalid surrogate
            char32_t ch2 = _read_utf16le();
            if (0xDC00 <= ch2 && ch2 < 0xE0000) {
              ch = (ch & 0x3FF) << 10 | (ch2 & 0x3FF);
              ch += 0x10000;
            } else {
              std::cerr << name << ".bin:" << ku << "-" << ten << ": invalid surrogate" << std::endl;
              ch = 0xFFFD;
            }
          } else if (0xDC00 <= ch &&ch < 0xE000) {
            std::cerr << name << ".bin:" << ku << "-" << ten << ": invalid surrogate" << std::endl;
            ch = 0xFFFD;
          }

          if (ch != 0xFFFD)
            charset->define(charset->kuten2index(kuten), ch);
        }
      }

      // def ファイルもある時は追加で読み込む
      include_file(name + ".def", true);

      return true;
    }

  private:
    bool process_line() {
      skip_space();
      if (!*r || *r == '#') {
        return true;
      } else if (starts_with_skip("SB94(")) {
        return process_command_declare(iso2022_size_sb94);
      } else if (starts_with_skip("SB96(")) {
        return process_command_declare(iso2022_size_sb96);
      } else if (starts_with_skip("MB94(")) {
        return process_command_declare(iso2022_size_mb94);
      } else if (starts_with_skip("MB96(")) {
        return process_command_declare(iso2022_size_mb96);
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
      } else if (starts_with_skip("include ")) {
        return process_command_include();
      } else if (starts_with_skip("savebin ")) {
        return process_command_savebin();
      } else if (starts_with_skip("loadbin ")) {
        return process_command_loadbin();
      } else {
        print_error() << "unrecognized command" << std::endl;
        return false;
      }
    }

    bool process_stream(std::istream& istr) {
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
      return !error_flag;
    }

  public:
    bool process(std::istream& istr, const char* title, iso2022_charset_callback callback, std::uintptr_t cbparam) {
      this->title = title;
      this->callback = callback;
      this->cbparam = cbparam;

      this->iline = 0;
      bool const ret = process_stream(istr);
      if (current_charset && callback)
        callback(current_charset, current_csindex, cbparam);

      return ret;
    }
  };
}

bool iso2022_charset_registry::load_definition(std::istream& istr, const char* title, iso2022_charset_callback callback, std::uintptr_t cbparam) {
  iso2022_definition_reader reader(*this);
  return reader.process(istr, title, callback, cbparam);
}
bool iso2022_charset_registry::load_definition(const char* filename, iso2022_charset_callback callback, std::uintptr_t cbparam) {
  std::ifstream ifs(filename);
  if (!ifs) {
    std::cerr << "contra: failed to open charset definition file \"" << filename << "\"." << std::endl;
    return false;
  } else {
    return this->load_definition(ifs, filename, callback, cbparam);
  }
}

iso2022_charset_registry& iso2022_charset_registry::instance() {
  static bool initialized = false;
  static iso2022_charset_registry instance;
  if (!initialized) {
    initialized = true;
    instance.load_definition("res/iso2022.def");
    instance.load_definition("res/iso2022-jis.def");
  }
  return instance;
}
