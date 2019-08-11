// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_iso2022_hpp
#define contra_iso2022_hpp
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <mwg/except.h>
#include "contradef.hpp"

namespace contra {

  static constexpr char32_t undefined_code = 0xFFFFFFFF;
  static constexpr char32_t invalid_code = 0xFFFFFFFE;
  static constexpr char32_t multichar_conv = 0xFFFFFFFD;

  typedef std::uint32_t charset_t;

  inline constexpr charset_t iso2022_unspecified = 0xFFFFFFFF;
  inline constexpr charset_t iso2022_charset_db   = charflag_iso2022_db;
  inline constexpr charset_t iso2022_charset_drcs = charflag_iso2022_drcs;
  inline constexpr charset_t iso2022_94_iso646_usa = 0;
  inline constexpr charset_t iso2022_96_iso8859_1  = 1;
  inline constexpr charset_t iso2022_94_vt100_acs  = 2;

  inline constexpr charset_t code2cssize(char32_t code)  {
    return code & charflag_iso2022_db ? 96 * 96 : 96;
  }
  inline constexpr charset_t code2charset(char32_t code, charset_t cssize)  {
    return (code & charflag_iso2022_mask_code) / cssize |
      (code & charflag_iso2022_mask_flag);
  }
  inline constexpr charset_t code2charset(char32_t code)  {
    return code2charset(code, code2cssize(code));
  }
  inline constexpr bool is_iso2022_graphics(char32_t code) {
    return charflag_iso2022_graphics_beg <= code && code < charflag_iso2022_graphics_end;
  }

  enum iso2022_charset_size {
    iso2022_size_sb94,
    iso2022_size_sb96,
    iso2022_size_mb94,
    iso2022_size_mb96,
  };

  struct iso2022_charset {
    iso2022_charset_size type;
    int number_of_bytes;
    std::string seq;
    std::string reg;
    std::string name;

  private:
    std::vector<char32_t> map;
    std::unordered_map<char32_t, std::vector<char32_t>> multimap;

  public:
    std::uint32_t kuten2index(std::uint32_t kuten) const {
      std::uint32_t index = 0;
      std::uint32_t factor = 1;
      while (kuten) {
        index += ((kuten & 0xFF) - 0x20) * factor;
        kuten >>= 8;
        factor *= 96;
      }
      return index;
    }

    char32_t const& kuten2u(std::uint32_t kuten) const {
      return map[kuten2index(kuten)];
    }
    char32_t& kuten2u(std::uint32_t kuten) {
      return map[kuten2index(kuten)];
    }

  private:
    static constexpr std::size_t zpow(std::size_t base, int power) {
      std::size_t size = 1;
      for (int i = 0; i < power; i++) size *= base;
      return size;
    }

  public:
    iso2022_charset(iso2022_charset_size type, int number_of_bytes = -1) {
      if (number_of_bytes < 0) {
        number_of_bytes = 1;
        if (type == iso2022_size_mb94 || type == iso2022_size_mb96)
          number_of_bytes = 2;
      }

      this->type = type;
      this->number_of_bytes = number_of_bytes;
      std::size_t const size = zpow(96, number_of_bytes);
      map.resize(size, undefined_code);
    }

  public:
    void initialize_iso646_usa() {
      std::iota(map.begin(), map.end(), (char32_t) ascii_sp);
    }
    void initialize_iso8859_1() {
      std::iota(map.begin(), map.end(), (char32_t) ascii_nbsp);
    }
    void define(std::uint32_t index, char32_t u) {
      mwg_assert(u != multichar_conv);
      map[index] = u;
      multimap[index].clear();
    }
    void define(std::uint32_t index, std::vector<char32_t> const& vec) {
      if (vec.size() == 1) {
        define(index, vec[0]);
      } else {
        map[index] = multichar_conv;
        multimap[index] = vec;
      }
    }
    void define(std::uint32_t index, std::vector<char32_t>&& vec) {
      if (vec.size() == 1) {
        define(index, vec[0]);
      } else {
        map[index] = multichar_conv;
        multimap[index] = std::move(vec);
      }
    }
    void load_other_charset(iso2022_charset const& other) {
      mwg_assert(this != &other);
      mwg_assert(other.number_of_bytes == this->number_of_bytes,
        "other.number_of_bytes=%d vs this->number_of_bytes=%d",
        other.number_of_bytes, this->number_of_bytes);
      for (std::size_t i = 0; i < map.size(); i++) {
        if (other.map[i] == undefined_code) continue;
        if (this->map[i] == multichar_conv)
          this->multimap[i].clear();

        this->map[i] = other.map[i];
        if (other.map[i] == multichar_conv) {
          auto const it = other.multimap.find(i);
          mwg_assert(it != other.multimap.end());
          this->multimap[i] = it->second;
        }
      }
    }

  public:
    bool get_chars(std::vector<char32_t>& vec, std::uint32_t index) const {
      char32_t const a = map[index];
      if (a == undefined_code) {
        return false;
      } else if (a == multichar_conv) {
        auto const it = multimap.find(index);
        mwg_assert(it != multimap.end());
        vec = it->second;
        return true;
      } else {
        vec.clear();
        vec.push_back(a);
        return true;
      }
    }
  };

  typedef void (*iso2022_charset_callback)(iso2022_charset const*, std::uintptr_t cbparam);

  class iso2022_charset_registry {
    std::vector<iso2022_charset> m_category_sb;
    std::vector<iso2022_charset> m_category_mb;
    std::vector<iso2022_charset> m_category_sb_drcs;
    std::vector<iso2022_charset> m_category_mb_drcs;

    charset_t m_sb94[160]; // ISO-IR 94
    charset_t m_sb96[80];  // ISO-IR 96
    charset_t m_mb94[80];  // ISO-IR 94^2
    charset_t m_sb94_drcs[80]; // DRCS 94
    charset_t m_sb96_drcs[80]; // DRCS 96
    charset_t m_mb94_drcs[80]; // DRCS 94^2
    charset_t m_mb96_drcs[80]; // DRCS 96^2

    typedef std::unordered_map<std::string, charset_t> dictionary_type;
    dictionary_type m_dict_sb94;
    dictionary_type m_dict_sb96;
    dictionary_type m_dict_db94;
    dictionary_type m_dict_db96;

  public:
    iso2022_charset_registry() {
      std::fill(std::begin(m_sb94), std::end(m_sb94), iso2022_unspecified);
      std::fill(std::begin(m_sb96), std::end(m_sb96), iso2022_unspecified);
      std::fill(std::begin(m_mb94), std::end(m_mb94), iso2022_unspecified);

      // DRCS (DRCSMM 領域合計 160 区については予め割り当てて置く事にする)
      std::iota(std::begin(m_sb94_drcs), std::end(m_sb94_drcs), iso2022_charset_drcs | 0);
      m_category_sb_drcs.insert(m_category_sb_drcs.end(), (std::size_t) 80, iso2022_charset(iso2022_size_sb94));
      std::iota(std::begin(m_sb96_drcs), std::end(m_sb96_drcs), iso2022_charset_drcs | 80);
      m_category_sb_drcs.insert(m_category_sb_drcs.end(), (std::size_t) 80, iso2022_charset(iso2022_size_sb96));
      std::fill(std::begin(m_mb94_drcs), std::end(m_mb94_drcs), iso2022_unspecified);
      std::fill(std::begin(m_mb96_drcs), std::end(m_mb96_drcs), iso2022_unspecified);

      // initialize default charset
      {
        iso2022_charset charset(iso2022_size_sb94, 1);
        charset.seq = "B";
        charset.initialize_iso646_usa();
        m_category_sb.emplace_back(std::move(charset));
      }

      {
        iso2022_charset charset(iso2022_size_sb96, 1);
        charset.seq = "A";
        charset.initialize_iso8859_1();
        m_category_sb.push_back(charset);
      }

      // {■ファイルから読み取って定義する
      //   iso2022_charset charset;
      //   charset.type = iso2022_size_sb94;
      //   charset.seq = "0";
      //   charset.initialize_iso8859_1();
      //   m_category_sb.push_back(charset);
      // }

      m_sb94[ascii_B - 0x30] = iso2022_94_iso646_usa;
      m_sb96[ascii_A - 0x30] = iso2022_96_iso8859_1;
      m_sb94[ascii_0 - 0x30] = iso2022_94_vt100_acs;
      {
        iso2022_charset charset(iso2022_size_sb94, 1);
        charset.seq = "0";
        m_category_sb.emplace_back(std::move(charset));
      }
    }

  private:
    mutable std::string m_designator_buff;
    template<typename Char>
    charset_t find_charset_from_dict(
      Char const* intermediate, std::size_t intermediate_size, byte final_char, dictionary_type const& dict
    ) const {
      m_designator_buff.resize(intermediate_size + 1);
      std::copy(intermediate, intermediate + intermediate_size, m_designator_buff.begin());
      m_designator_buff.back() = final_char;
      auto const it = dict.find(m_designator_buff);
      if (it != dict.end()) return it->second;
      return iso2022_unspecified;
    }

  public:
    template<typename Char>
    charset_t resolve_designator(iso2022_charset_size type, Char const* intermediate, std::size_t intermediate_size, byte final_char) const {
      if (final_char < 0x30 || 0x7E < final_char)
        return iso2022_unspecified;
      byte const f = final_char - 0x30;

      // F型 (Ft型 = IR文字集合, Fp型 = 私用文字集合)
      if (intermediate_size == 0) {
        switch (type) {
        case iso2022_size_sb94: return m_sb94[f];
        case iso2022_size_sb96: return m_sb96[f];
        case iso2022_size_mb94: return m_mb94[f];
        case iso2022_size_mb96: break;
        default:
          mwg_check(0, "BUG unexpected charset size");
          return iso2022_unspecified;
        }
      } else if (intermediate_size == 1) {
        // 1F型 (1Ft型 = IR 94文字集合の追加分, 1Fp型 = 私用文字集合)
        if (intermediate[0] == ascii_exclamation && type == iso2022_size_sb94)
          return m_sb94[80 + f];

        // 0F型 = DRCS
        if (intermediate[0] == ascii_sp) {
          switch (type) {
          case iso2022_size_sb94: return m_sb94_drcs[f];
          case iso2022_size_sb96: return m_sb96_drcs[f];
          case iso2022_size_mb94: return m_mb94_drcs[f];
          case iso2022_size_mb96: return m_mb96_drcs[f];
          default:
            mwg_check(0, "BUG unexpected charset size");
            return iso2022_unspecified;
          }
        }
      }

      // その他 DECDLD 等でユーザによって追加された分
      switch (type) {
      case iso2022_size_sb94: return find_charset_from_dict(intermediate, intermediate_size, final_char, m_dict_sb94);
      case iso2022_size_sb96: return find_charset_from_dict(intermediate, intermediate_size, final_char, m_dict_sb96);
      case iso2022_size_mb94: return find_charset_from_dict(intermediate, intermediate_size, final_char, m_dict_db94);
      case iso2022_size_mb96: return find_charset_from_dict(intermediate, intermediate_size, final_char, m_dict_db96);
      default:
        mwg_check(0, "BUG unexpected charset size");
        return iso2022_unspecified;
      }
    }

    charset_t resolve_designator(iso2022_charset_size type, std::string const& designator) const {
      mwg_assert(designator.size());
      return resolve_designator(type, &designator[0], designator.size() - 1, designator.back());
    }

  private:
    bool register_designator(iso2022_charset_size type, std::string const& designator, charset_t cs) {
      mwg_assert(designator.size());
      if (designator.back() < 0x30 || 0x7E < designator.back()) return false;
      byte const f = designator.back() - 0x30;

      if (designator.size() == 1) {
        switch (type) {
        case iso2022_size_sb94: m_sb94[f] = cs; return true;
        case iso2022_size_sb96: m_sb96[f] = cs; return true;
        case iso2022_size_mb94: m_mb94[f] = cs; return true;
        case iso2022_size_mb96: break;
        default: mwg_assert(0);
        }
      } else if (designator.size() == 2) {
        if (designator[0] == ascii_exclamation && type == iso2022_size_sb94) {
          m_sb94[80 + f] = cs;
          return true;
        }

        if (designator[0] == ascii_sp) {
          switch (type) {
          case iso2022_size_sb94: m_sb94_drcs[f] = cs; return true;
          case iso2022_size_sb96: m_sb96_drcs[f] = cs; return true;
          case iso2022_size_mb94: m_mb94_drcs[f] = cs; return true;
          case iso2022_size_mb96: m_mb96_drcs[f] = cs; return true;
          default: mwg_assert(0);
          }
        }
      }

      switch (type) {
      case iso2022_size_sb94: m_dict_sb94[designator] = cs; return true;
      case iso2022_size_sb96: m_dict_sb96[designator] = cs; return true;
      case iso2022_size_mb94: m_dict_db94[designator] = cs; return true;
      case iso2022_size_mb96: m_dict_db96[designator] = cs; return true;
      default: mwg_assert(0);
      }
      return false;
    }

  public:
    charset_t register_charset(iso2022_charset&& charset, bool drcs) {
      mwg_check(charset.seq.size(), "charset with empty designator cannot be registered.");
      mwg_check(charset.number_of_bytes <= 2, "94^n/96^n charsets (n >= 3) are not supported.");
      charset_t cs = resolve_designator(charset.type, charset.seq);
      if (cs != iso2022_unspecified) {
        std::vector<iso2022_charset>* category = nullptr;
        if (cs & charflag_iso2022_db)
          category = cs & charflag_iso2022_drcs ? &m_category_mb_drcs : &m_category_mb;
        else
          category = cs & charflag_iso2022_drcs ? &m_category_sb_drcs : &m_category_sb;

        charset_t const index = cs & charflag_iso2022_mask_code;
        mwg_assert(index < category->size(), "index=%d", index);
        (*category)[index] = std::move(charset);
        return cs;
      }

      std::vector<iso2022_charset>* category = nullptr;
      std::size_t max_count = 0;
      switch (charset.type) {
      case iso2022_size_sb94:
      case iso2022_size_sb96:
        category = drcs ? &m_category_sb_drcs : &m_category_sb;
        cs = 0;
        max_count = (charflag_iso2022_mask_code + 1) / 96;
        break;
      case iso2022_size_mb94:
      case iso2022_size_mb96:
        category = drcs ? &m_category_mb_drcs : &m_category_mb;
        cs = charflag_iso2022_db;
        max_count = (charflag_iso2022_mask_code + 1) / (96 * 96);
        break;
      default: mwg_check(0);
      }
      if (drcs) cs |= charflag_iso2022_drcs;

      if (category->size() >= max_count) {
        std::cerr << "reached maximal number of registered charset." << std::endl;
        return iso2022_unspecified;
      }

      cs |= category->size();
      auto const& charset_ = category->emplace_back(std::move(charset));
      register_designator(charset_.type, charset_.seq, cs);
      return cs;
    }

    iso2022_charset* charset(charset_t cs) {
      std::vector<iso2022_charset>* category = nullptr;
      if (cs & charflag_iso2022_db)
        category = cs & charflag_iso2022_drcs ? &m_category_mb_drcs : &m_category_mb;
      else
        category = cs & charflag_iso2022_drcs ? &m_category_sb_drcs : &m_category_sb;

      charset_t const index = cs & charflag_iso2022_mask_code;
      mwg_assert(index < category->size());
      return &(*category)[index];
    }
    iso2022_charset const* charset(charset_t cs) const {
      return const_cast<iso2022_charset_registry*>(this)->charset(cs);
    }

    bool load_definition(std::istream& istr, const char* title, iso2022_charset_callback callback = nullptr, std::uintptr_t cbparam = 0);
    bool load_definition(const char* filename, iso2022_charset_callback callback = nullptr, std::uintptr_t cbparam = 0);

    static iso2022_charset_registry& instance();
  };
}

#endif
