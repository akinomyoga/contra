#include <cstdint>
#include <vector>
#include <type_traits>

namespace contra {
namespace util {

  namespace flags_detail {
    template<typename Flags, typename Extends>
    struct import_from: std::false_type {};

    struct flags_base {};
    template<typename Flags>
    constexpr bool is_flags = std::is_base_of<flags_base, Flags>::value;
    template<typename Flags, typename T = void>
    using enable_flags = typename std::enable_if<is_flags<Flags>, T>::type;

    template<typename Flags, typename Other>
    constexpr bool is_rhs = is_flags<Flags> && (
      std::is_same<Other, Flags>::value ||
      std::is_integral<Other>::value && (sizeof(Other) <= sizeof(Flags)) ||
      import_from<Flags, Other>::value);

    template<typename Flags, typename Other>
    using enable_lhs = typename std::enable_if<is_rhs<Flags, Other>, Flags>::type;

    template<typename Flags1, typename Flags2>
    using result_t = typename std::enable_if<
      is_rhs<Flags1, Flags2> || is_rhs<Flags2, Flags1>,
      typename std::conditional<is_rhs<Flags1, Flags2>, Flags1, Flags2>::type>::type;

    template<typename Flags, bool = is_flags<Flags> >
    struct underlying { typedef Flags type; };
    template<typename Flags>
    struct underlying<Flags, true> { typedef typename Flags::underlying_type type; };

    template<typename Flags, bool = is_flags<Flags> >
    constexpr auto peel(Flags const& value) { return (typename underlying<Flags>::type) value; }

    template<typename Flags1, typename Flags2>
    constexpr flags_detail::result_t<Flags1, Flags2>
    operator|(Flags1 const& lhs, Flags2 const& rhs) { return { peel(lhs) | peel(rhs)}; }
    template<typename Flags1, typename Flags2>
    constexpr flags_detail::result_t<Flags1, Flags2>
    operator&(Flags1 const& lhs, Flags2 const& rhs) { return { peel(lhs) & peel(rhs)}; }
    template<typename Flags1, typename Flags2>
    constexpr flags_detail::result_t<Flags1, Flags2>
    operator^(Flags1 const& lhs, Flags2 const& rhs) { return { peel(lhs) ^ peel(rhs)}; }
  }

  template<typename Unsigned, typename Tag>
  class flags_t: flags_detail::flags_base {
    Unsigned value;
  public:
    typedef Unsigned underlying_type;

    template<typename Other, typename = flags_detail::enable_lhs<flags_t, Other> >
    constexpr flags_t(Other const& value): value((Unsigned) value) {}

    constexpr explicit operator Unsigned() const { return value; }
    constexpr explicit operator bool() const { return value; }
    constexpr bool operator!() const { return !value; }
    constexpr flags_t operator~() const { return { ~value }; }

    template<typename Other>
    constexpr flags_detail::enable_lhs<flags_t, Other>&
    operator&=(Other const& rhs) { value &= (Unsigned) rhs; return *this; }
    template<typename Other>
    constexpr flags_detail::enable_lhs<flags_t, Other>&
    operator|=(Other const& rhs) { value |= (Unsigned) rhs; return *this; }
    template<typename Other>
    constexpr flags_detail::enable_lhs<flags_t, Other>&
    operator^=(Other const& rhs) { value ^= (Unsigned) rhs; return *this; }

    flags_t& reset(flags_t mask, flags_t value) {
      this->value = this->value & ~(Unsigned) mask | (Unsigned) value;
      return *this;
    }
  };

  using flags_detail::operator|;
  using flags_detail::operator^;
  using flags_detail::operator&;
}
}

namespace contra::ansi {
  typedef std::uint32_t color_t;
  typedef contra::util::flags_t<std::uint32_t, struct attr0_tag> attr0_t;
  typedef contra::util::flags_t<std::uint32_t, struct attr_tag>  attr_t;
  typedef contra::util::flags_t<std::uint32_t, struct aflags_tag> aflags_t;
  typedef contra::util::flags_t<std::uint32_t, struct xflags_tag> xflags_t;
}
namespace contra::util::flags_detail {
  template<> struct import_from<contra::ansi::attr_t, contra::ansi::attr0_t>: std::true_type {};
  template<> struct import_from<contra::ansi::aflags_t, contra::ansi::attr0_t>: std::true_type {};
}

namespace contra {
namespace ansi {

  // attr_t, aflags_t 共通属性

  constexpr attr0_t attr_weight_mask      = 3 << 18; // bit 18-19
  constexpr attr0_t attr_bold_set         = 1 << 18; // -+- SGR 1,2
  constexpr attr0_t attr_faint_set        = 2 << 18; //  |
  constexpr attr0_t attr_heavy_set        = 3 << 18; // -' (contra拡張)

  constexpr attr0_t attr_shape_mask       = 3 << 20; // bit 20-21
  constexpr attr0_t attr_italic_set       = 1 << 20; // -+- SGR 3,20
  constexpr attr0_t attr_fraktur_set      = 2 << 20; // -'

  constexpr attr0_t attr_underline_mask   = 7 << 22; // bit 22-24
  constexpr attr0_t attr_underline_single = 1 << 22; // -+- SGR 4,21
  constexpr attr0_t attr_underline_double = 2 << 22; //  |  SGR 21
  constexpr attr0_t attr_underline_curly  = 3 << 22; //  |  (kitty拡張)
  constexpr attr0_t attr_underline_dotted = 4 << 22; //  |  (mintty拡張)
  constexpr attr0_t attr_underline_dashed = 5 << 22; // -'  (mintty拡張)

  constexpr attr0_t attr_blink_mask       = 3 << 25; // bit 25-26
  constexpr attr0_t attr_blink_set        = 1 << 25; // -+- SGR 5,6
  constexpr attr0_t attr_rapid_blink_set  = 2 << 25; // -'

  constexpr attr0_t attr_inverse_set      = 1 << 27; // SGR 7
  constexpr attr0_t attr_invisible_set    = 1 << 28; // SGR 8
  constexpr attr0_t attr_strike_set       = 1 << 29; // SGR 9

  constexpr attr0_t attr_common_mask      = 0x3FFF0000;

  // attr_t
  constexpr attr_t attr_fg_mask  = 0x0000FF;
  constexpr attr_t attr_bg_mask  = 0x00FF00;
  constexpr int    attr_fg_shift = 0;
  constexpr int    attr_bg_shift = 8;
  constexpr attr_t attr_fg_set   = 0x010000;
  constexpr attr_t attr_bg_set   = 0x020000;
  constexpr attr_t attr_reserved_bit1 = 1 << 30;
  constexpr attr_t attr_extended = 1 << 31;
  constexpr int color_space_default      = 0;
  constexpr int color_space_transparent  = 1;
  constexpr int color_space_rgb          = 2;
  constexpr int color_space_cmy          = 3;
  constexpr int color_space_cmyk         = 4;
  constexpr int color_space_indexed      = 5;

  // aflags_t
  constexpr aflags_t aflags_fg_space_mask = 0x00000F;
  constexpr aflags_t aflags_bg_space_mask = 0x0000F0;
  constexpr aflags_t aflags_dc_space_mask = 0x000F00;
  constexpr int      aflags_fg_space_shift = 0;
  constexpr int      aflags_bg_space_shift = 4;
  constexpr int      aflags_dc_space_shift = 8;
  constexpr aflags_t aflags_reserved_bit1 = 1 << 16;
  constexpr aflags_t aflags_reserved_bit2 = 1 << 17;
  constexpr aflags_t aflags_reserved_bit3 = 1 << 30;
  constexpr aflags_t aflags_reserved_bit4 = 1 << 31;

  // xflags_t

  // bit 0-3: sup/sub
  constexpr xflags_t xflags_subsup_mask        = 3 << 0; // PLD,PLU
  constexpr xflags_t xflags_sub_set            = 1 << 0; //   反対の PLD/PLU でクリア
  constexpr xflags_t xflags_sup_set            = 2 << 0; //
  constexpr xflags_t xflags_mintty_subsup_mask = 3 << 2; // mintty SGR 73,74
  constexpr xflags_t xflags_mintty_sup         = 1 << 2; //   SGR(0) でクリア
  constexpr xflags_t xflags_mintty_sub         = 2 << 2; //

  // bit 6,7: DECDHL, DECDWL, DECSWL
  constexpr xflags_t xflags_decdhl_mask         = 0x3 << 6;
  constexpr xflags_t xflags_decdhl_single_width = 0x0 << 6;
  constexpr xflags_t xflags_decdhl_double_width = 0x1 << 6;
  constexpr xflags_t xflags_decdhl_upper_half   = 0x2 << 6;
  constexpr xflags_t xflags_decdhl_lower_half   = 0x3 << 6;

  // bit 8-10: SCO
  constexpr int     xflags_sco_shift      = 8;
  constexpr xflags_t xflags_sco_mask      = 0x7 << xflags_sco_shift;
  constexpr xflags_t xflags_sco_default   = 0x0 << xflags_sco_shift;
  constexpr xflags_t xflags_sco_rotate45  = 0x1 << xflags_sco_shift;
  constexpr xflags_t xflags_sco_rotate90  = 0x2 << xflags_sco_shift;
  constexpr xflags_t xflags_sco_rotate135 = 0x3 << xflags_sco_shift;
  constexpr xflags_t xflags_sco_rotate180 = 0x4 << xflags_sco_shift;
  constexpr xflags_t xflags_sco_rotate225 = 0x5 << xflags_sco_shift;
  constexpr xflags_t xflags_sco_rotate270 = 0x6 << xflags_sco_shift;
  constexpr xflags_t xflags_sco_rotate315 = 0x7 << xflags_sco_shift;

  // bit 11: DECSCA
  constexpr xflags_t xflags_decsca_protected = 1 << 11;

  // bit 12-15: SGR(ECMA-48:1986)
  constexpr xflags_t xflags_frame_mask       = 3 << 12;
  constexpr xflags_t xflags_frame_set        = 1 << 12; // -+- SGR 51,52
  constexpr xflags_t xflags_circle_set       = 2 << 12; // -'
  constexpr xflags_t xflags_overline_set     = 1 << 14; // --- SGR 53
  constexpr xflags_t xflags_proportional_set = 1 << 15; // --- SGR 26 (deprecated)

  // bit 16-19: SGR(ECMA-48:1986) ideogram decorations
  constexpr xflags_t xflags_ideogram_mask           = 0xF0000;
  constexpr xflags_t xflags_ideogram_line           = 0x80000;
  constexpr xflags_t xflags_ideogram_line_double    = 0x10000;
  constexpr xflags_t xflags_ideogram_line_left      = 0x20000;
  constexpr xflags_t xflags_ideogram_line_over      = 0x40000;
  constexpr xflags_t xflags_ideogram_line_single_rb = xflags_ideogram_line | 0x0 << 16; // -+- SGR 60-63
  constexpr xflags_t xflags_ideogram_line_double_rb = xflags_ideogram_line | 0x1 << 16; //  |      66-69
  constexpr xflags_t xflags_ideogram_line_single_lt = xflags_ideogram_line | 0x6 << 16; //  |
  constexpr xflags_t xflags_ideogram_line_double_lt = xflags_ideogram_line | 0x7 << 16; //  |
  constexpr xflags_t xflags_ideogram_line_single_lb = xflags_ideogram_line | 0x2 << 16; //  |
  constexpr xflags_t xflags_ideogram_line_double_lb = xflags_ideogram_line | 0x3 << 16; //  |
  constexpr xflags_t xflags_ideogram_line_single_rt = xflags_ideogram_line | 0x4 << 16; //  |
  constexpr xflags_t xflags_ideogram_line_double_rt = xflags_ideogram_line | 0x5 << 16; // -'
  constexpr xflags_t xflags_ideogram_stress         = 0x10000; // --- SGR 64

  // bit 20-23: RLogin SGR(60-63) 左右の線
  constexpr xflags_t xflags_rlogin_ideogram_mask = 0x1F << 20;
  constexpr xflags_t xflags_rlogin_single_rline  = 1 << 20; // SGR 8460
  constexpr xflags_t xflags_rlogin_double_rline  = 1 << 21; // SGR 8461
  constexpr xflags_t xflags_rlogin_single_lline  = 1 << 22; // SGR 8462
  constexpr xflags_t xflags_rlogin_double_lline  = 1 << 23; // SGR 8463
  // bit 24: RLogin SGR(64) 二重打ち消し線
  constexpr xflags_t xflags_rlogin_double_strike = 1 << 24; // SGR 8464

  // bit 25,26: SPA, SSA
  constexpr xflags_t xflags_spa_protected         = 1 << 25;
  constexpr xflags_t xflags_ssa_selected          = 1 << 26;
  // bit 27,28: DAQ
  constexpr xflags_t xflags_daq_guarded           = 1 << 27;
  constexpr xflags_t xflags_daq_protected         = 1 << 28;
  // constexpr int     xflags_daq_shift = 29;
  // constexpr xflags_t xflags_daq_mask  = (xflags_t) 0x3 << daq_shift;
  // constexpr xflags_t xflags_daq_character_input   = (xflags_t) 2  << daq_shift;
  // constexpr xflags_t xflags_daq_numeric_input     = (xflags_t) 3  << daq_shift;
  // constexpr xflags_t xflags_daq_alphabetic_input  = (xflags_t) 4  << daq_shift;
  // constexpr xflags_t xflags_daq_input_align_right = (xflags_t) 6  << daq_shift;
  // constexpr xflags_t xflags_daq_input_reversed    = (xflags_t) 7  << daq_shift;
  // constexpr xflags_t xflags_daq_zero_fill         = (xflags_t) 8  << daq_shift;
  // constexpr xflags_t xflags_daq_space_fill        = (xflags_t) 9  << daq_shift;
  // constexpr xflags_t xflags_daq_tabstop           = (xflags_t) 10 << daq_shift;

  constexpr xflags_t xflags_reserved_bit1 = 1 << 4;
  constexpr xflags_t xflags_reserved_bit2 = 1 << 5;
  constexpr xflags_t xflags_fg_extended = 1 << 29;
  constexpr xflags_t xflags_bg_extended = 1 << 30;
  constexpr xflags_t xflags_dc_extended = 1 << 31;

  constexpr xflags_t xflags_qualifier_mask = xflags_decsca_protected | xflags_spa_protected | xflags_ssa_selected | xflags_daq_guarded | xflags_daq_protected;

  // 以下に \e[m でクリアされない物を列挙する。
  // SGR(9903)-SGR(9906) で提供している decdhl_mask については \e[m でクリアできる事にする。
  constexpr xflags_t xflags_non_sgr_mask = xflags_subsup_mask | xflags_sco_mask | xflags_qualifier_mask;

  struct attribute {
    aflags_t aflags = 0;
    xflags_t xflags = 0;
    color_t fg = 0;
    color_t bg = 0;
    color_t dc = 0;
  };

  class attr_table {
    struct entry {
      attribute attr;
      attr_t listp;
      entry(attribute const& attr): attr(attr), listp(0) {}
    };

    std::vector<entry> table;
    attr_t freep = 0;

    std::uint32_t max_size = 0x1000;

  public:
    attr_t save(attribute const& attr) {
      if (freep) {
        attr_t const ret = freep;
        entry& ent = table[(std::uint32_t) (ret & ~attr_extended)];
        freep = ent.listp;
        ent.attr = attr;
        ent.listp = 0;
        return ret;
      } else if (table.size() < max_size) {
        attr_t const ret = attr_extended | (std::uint32_t) table.size();
        table.emplace_back(attr);
        return ret;
      } else {
        // todo: 容量不足
        return 0;
      }
    }

    // todo: mark/sweep
  };

  struct attribute_builder {
    attr_table&    m_table;
    attribute     m_attribute;
    std::uint32_t m_attribute_version = 0;
    mutable attr_t        m_attr = 0;
    mutable std::uint32_t m_attr_version = 0;

  public:
    attribute_builder(attr_table& table): m_table(table) {}

  public:
    attr_t attr() const {
      if (!(m_attr & attr_extended))
        return m_attr;
      if (m_attribute_version == m_attr_version)
        return m_attr;

      if (m_attribute.xflags) {
        m_attr = m_table.save(m_attribute);
        m_attr_version = m_attribute_version;
      } else {
        m_attr = (std::uint32_t) m_attribute.aflags & attr_common_mask;
        if (m_attribute.aflags & aflags_fg_space_mask)
          m_attr |= attr_fg_set | m_attribute.fg << attr_fg_shift;
        if (m_attribute.aflags & aflags_bg_space_mask)
          m_attr |= attr_bg_set | m_attribute.bg << attr_bg_shift;
      }
      return m_attr;
    }

  private:
    void extend() {
      m_attribute.aflags = (std::uint32_t) m_attr & attr_common_mask;
      m_attribute.xflags = 0;
      if (m_attr & attr_fg_set) {
        m_attribute.aflags |= color_space_indexed << aflags_fg_space_shift;
        m_attribute.fg = (std::uint32_t) (m_attr & attr_fg_mask) >> attr_fg_shift;
      }
      if (m_attr & attr_bg_set) {
        m_attribute.aflags |= color_space_indexed << aflags_bg_space_shift;
        m_attribute.bg = (std::uint32_t) (m_attr & attr_bg_mask) >> attr_bg_shift;
      }
      m_attr_version = 0;
      m_attribute_version = 0;
    }

  public:
    void set_fg(color_t fg, int space) {
      if (!(m_attr & attr_extended)) {
        if (space == color_space_default) {
          m_attr &= ~(attr_fg_set | attr_fg_mask);
          return;
        } else if (space == color_space_indexed) {
          m_attr = m_attr & ~attr_fg_mask | attr_fg_set | fg << attr_fg_shift;
          return;
        }
        extend();
      }

      m_attribute.fg = fg;
      m_attribute.aflags.reset(aflags_fg_space_mask, space << aflags_fg_space_shift);
      if (space == color_space_default || space == color_space_indexed)
        m_attribute.xflags &= ~xflags_fg_extended;
      else
        m_attribute.xflags |= xflags_fg_extended;
      m_attribute_version++;
    }
    void set_bg(color_t bg, int space) {
      if (!(m_attr & attr_extended)) {
        if (space == color_space_default) {
          m_attr &= ~(attr_bg_set | attr_bg_mask);
          return;
        } else if (space == color_space_indexed) {
          m_attr = m_attr & ~attr_bg_mask | attr_bg_set | bg << attr_bg_shift;
          return;
        }
        extend();
      }

      m_attribute.bg = bg;
      m_attribute.aflags.reset(aflags_bg_space_mask, space << aflags_bg_space_shift);
      if (space == color_space_default || space == color_space_indexed)
        m_attribute.xflags &= ~xflags_bg_extended;
      else
        m_attribute.xflags |= xflags_bg_extended;
      m_attribute_version++;
    }
    void set_dc(color_t dc, int space) {
      if (!(m_attr & attr_extended)) {
        if (space == color_space_default) return;
        extend();
      }

      m_attribute.dc = dc;
      m_attribute.aflags.reset(aflags_dc_space_mask, space << aflags_dc_space_shift);
      if (space == color_space_default)
        m_attribute.xflags &= ~xflags_dc_extended;
      else
        m_attribute.xflags |= xflags_dc_extended;
      m_attribute_version++;
    }

  };

}
}

namespace test_flags {
  using namespace contra::util;
  typedef flags_t<std::uint32_t, struct f1_tag> f1_t;
  typedef flags_t<std::uint32_t, struct f2_tag> f2_t;
  void test1() {
    static_assert(flags_detail::is_flags<f1_t>);
    static_assert(flags_detail::is_rhs<f1_t, int>);
    constexpr f1_t f1_hello = 1;
    constexpr f2_t f2_hello = 1;

    constexpr f1_t f1_hello_world = f1_hello | 2;
    constexpr f1_t f1_hello_ringo = f1_hello & 3;
    constexpr f1_t f1_hello_orange = f1_hello ^ 4;

    f1_t v1 = f1_hello_world | f1_hello_ringo;
    v1 |= f1_hello_orange;
    v1 &= ~f1_hello_orange;
    //v1 &= f2_hello; // Error
  }
}

using namespace contra::ansi;

int main() {
  attr_table table;
  attribute_builder buffer(table);
  buffer.set_fg(10, 5);
  buffer.set_fg(0, 0);

  return 0;
}
