// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_dict_hpp
#define contra_dict_hpp
#include <cstdint>
#include <cstdio>
#include <mwg/except.h>
#include "contradef.hpp"
#include "enc.utf8.hpp"

namespace contra {
namespace dict {
  using namespace ::contra::ansi;

  typedef std::uint32_t color_t;

  // bgra 0xAARRGGBB GDI+
  // rgb  0x00BBGGRR 38:2, GDI (COLORREF)
  // rgba 0xAABBGGRR       GDI (COLORREF?)
  // cmy  0x00YYMMCC 38:3
  // cmyk 0xKKYYMMCC 38:4
  constexpr color_t rgb(byte r, byte g, byte b) {
    return r | g << 8 | b << 16;
  }
  constexpr color_t rgba(byte r, byte g, byte b, byte a = 0xFF) {
    return r | g << 8 | b << 16 | a << 24;
  }
  constexpr color_t cmy(byte c, byte m, byte y) {
    return c | m << 8 | y << 16;
  }
  constexpr color_t rgba2rgb(color_t value) {
    return 0x00FFFFFF & value;
  }
  constexpr color_t rgb2rgba(color_t value) {
    return 0xFF000000 | value;
  }
  constexpr color_t cmy2rgba(color_t value) {
    return 0xFF000000 | ~value;
  }
  constexpr color_t cmyk2rgba(color_t value) {
    value = ~value;
    byte const alpha = value >> 24;
    return rgba(
      (0xFF & value      ) * alpha / 255,
      (0xFF & value >> 8 ) * alpha / 255,
      (0xFF & value >> 16) * alpha / 255);
  }
  constexpr color_t bgr(byte r, byte g, byte b) {
    return r << 16 | g << 8 | b;
  }
  constexpr color_t rgba2bgr(color_t value) {
    return bgr(value, value >> 8, value >> 16);
  }
  constexpr byte rgba2r(color_t value) { return value & 0xFF; }
  constexpr byte rgba2g(color_t value) { return value >> 8 & 0xFF; }
  constexpr byte rgba2b(color_t value) { return value >> 16 & 0xFF; }
  constexpr byte rgba2a(color_t value) { return value >> 24 & 0xFF; }

  typedef std::uint32_t aflags_t;
  typedef std::uint32_t xflags_t;

  struct attribute_t {
    enum attribute_flags {
      /*?lwiki
       * Bits 0-7 and 9-15 of `attribute_t, aflags_t`
       * specify foreground and background color, respectively.
       * The following constants can be used to extract/store
       * the corresponding bits of the foreground and background colors.
       * - @const fg_color_mask, bg_color_mask
       * - @const fg_color_shift, bg_color_shift
       *
       * When the attribute is used as `attribute_t`,
       * these values hold the index of the represented color.
       *
       * When the attribute is used as `aflags_t` combined with `xflags_t`,
       * these values hold the color spec identifiers.
       * This is the list of color spec identifiers:
       * - @const color_space_default
       * - @const color_space_transparent
       * - @const color_space_rgb
       * - @const color_space_cmy
       * - @const color_space_cmyk
       * - @const color_space_indexed
       */
      is_fg_indexed            = (aflags_t) 1 << 16,
      is_bg_indexed            = (aflags_t) 1 << 17,
      fg_color_mask            = 0x00FF,
      bg_color_mask            = 0xFF00,
      fg_color_shift           = 0,
      bg_color_shift           = 8,
      color_space_default      = 0,
      color_space_transparent  = 1,
      color_space_rgb          = 2,
      color_space_cmy          = 3,
      color_space_cmyk         = 4,
      color_space_indexed      = 5,

      aflags_weight_mask      = (aflags_t) 3 << 18, // bit 18-19
      is_bold_set             = (aflags_t) 1 << 18, // -+- SGR 1,2
      is_faint_set            = (aflags_t) 2 << 18, //  |
      is_heavy_set            = (aflags_t) 3 << 18, // -' (contra拡張)

      aflags_shape_mask       = (aflags_t) 3 << 20, // bit 19-20
      is_italic_set           = (aflags_t) 1 << 20, // -+- SGR 3,20
      is_fraktur_set          = (aflags_t) 2 << 20, // -'

      aflags_underline_mask   = (aflags_t) 7 << 22, // bit 22-24
      underline_single        = (aflags_t) 1 << 22, // -+- SGR 4,21
      underline_double = (aflags_t) 2 << 22, //  |  SGR 21
      underline_curly  = (aflags_t) 3 << 22, //  |  (kitty拡張)
      underline_dotted = (aflags_t) 4 << 22, //  |  (mintty拡張)
      underline_dashed = (aflags_t) 5 << 22, // -'  (mintty拡張)

      aflags_blink_mask       = (aflags_t) 3 << 25, // bit 25-26
      is_blink_set            = (aflags_t) 1 << 25, // -+- SGR 5,6
      is_rapid_blink_set      = (aflags_t) 2 << 25, // -'

      is_inverse_set          = (aflags_t) 1 << 27, // SGR 7
      is_invisible_set        = (aflags_t) 1 << 28, // SGR 8
      is_strike_set           = (aflags_t) 1 << 29, // SGR 9

      aflags_reserved_bit1    = (aflags_t) 1 << 30,

      // only valid for attribute_t
      has_extended_attribute  = (aflags_t) 1 << 31,
    };

    enum extended_flags {
      // bit 0-3: SGR 10-19
      ansi_font_mask  = 0x0000000F,
      ansi_font_shift = 0,

      // bit 4,5: PLD, PLU
      xflags_subsup_mask = (xflags_t) 3 << 4,
      is_sub_set  = (xflags_t) 1 << 4,
      is_sup_set  = (xflags_t) 1 << 5,

      // bit 6,7: DECDHL, DECDWL, DECSWL
      decdhl_mask         = (xflags_t) 0x3 << 6,
      decdhl_single_width = (xflags_t) 0x0 << 6,
      decdhl_double_width = (xflags_t) 0x1 << 6,
      decdhl_upper_half   = (xflags_t) 0x2 << 6,
      decdhl_lower_half   = (xflags_t) 0x3 << 6,

      // bit 8-10: SCO
      sco_shift     = 8,
      sco_mask      = (xflags_t) 0x7 << sco_shift,
      sco_default   = (xflags_t) 0x0 << sco_shift,
      sco_rotate45  = (xflags_t) 0x1 << sco_shift,
      sco_rotate90  = (xflags_t) 0x2 << sco_shift,
      sco_rotate135 = (xflags_t) 0x3 << sco_shift,
      sco_rotate180 = (xflags_t) 0x4 << sco_shift,
      sco_rotate225 = (xflags_t) 0x5 << sco_shift,
      sco_rotate270 = (xflags_t) 0x6 << sco_shift,
      sco_rotate315 = (xflags_t) 0x7 << sco_shift,

      // bit 11: DECSCA
      decsca_protected = (xflags_t) 1 << 11,

      // bit 12-15: SGR(ECMA-48:1986)
      xflags_frame_mask   = (xflags_t) 3 << 12,
      is_frame_set        = (xflags_t) 1 << 12, // -+- SGR 51,52
      is_circle_set       = (xflags_t) 1 << 13, // -'
      is_overline_set     = (xflags_t) 1 << 14, // --- SGR 53
      is_proportional_set = (xflags_t) 1 << 15, // --- SGR 26 (deprecated)

      // bit 16-19: SGR(ECMA-48:1986) ideogram decorations
      is_ideogram_mask           = (xflags_t) 0xF0000,
      is_ideogram_line           = (xflags_t) 0x80000,
      is_ideogram_line_double    = (xflags_t) 0x10000,
      is_ideogram_line_left      = (xflags_t) 0x20000,
      is_ideogram_line_over      = (xflags_t) 0x40000,
      is_ideogram_line_single_rb = is_ideogram_line | (xflags_t) 0x0 << 16, // -+- SGR 60-63
      is_ideogram_line_double_rb = is_ideogram_line | (xflags_t) 0x1 << 16, //  |      66-69
      is_ideogram_line_single_lt = is_ideogram_line | (xflags_t) 0x6 << 16, //  |
      is_ideogram_line_double_lt = is_ideogram_line | (xflags_t) 0x7 << 16, //  |
      is_ideogram_line_single_lb = is_ideogram_line | (xflags_t) 0x2 << 16, //  |
      is_ideogram_line_double_lb = is_ideogram_line | (xflags_t) 0x3 << 16, //  |
      is_ideogram_line_single_rt = is_ideogram_line | (xflags_t) 0x4 << 16, //  |
      is_ideogram_line_double_rt = is_ideogram_line | (xflags_t) 0x5 << 16, // -'
      is_ideogram_stress         = (xflags_t) 0x10000, // --- SGR 64

      // bit 20-23: RLogin SGR(60-63) 左右の線
      rlogin_ideogram_mask = (xflags_t) 0x1F << 20,
      rlogin_single_rline  = (xflags_t) 1 << 20, // SGR 8460
      rlogin_double_rline  = (xflags_t) 1 << 21, // SGR 8461
      rlogin_single_lline  = (xflags_t) 1 << 22, // SGR 8462
      rlogin_double_lline  = (xflags_t) 1 << 23, // SGR 8463
      // bit 24: RLogin SGR(64) 二重打ち消し線
      rlogin_double_strike = (xflags_t) 1 << 24, // SGR 8464

      // bit 25,26: SPA, SSA
      spa_protected         = (xflags_t) 1 << 25,
      ssa_selected          = (xflags_t) 1 << 26,
      // bit 27,28: DAQ
      daq_guarded           = (xflags_t) 1 << 27,
      daq_protected         = (xflags_t) 1 << 28,
      // daq_shift = 29,
      // daq_mask  = (xflags_t) 0x3 << daq_shift,
      // daq_character_input   = (xflags_t) 2  << daq_shift,
      // daq_numeric_input     = (xflags_t) 3  << daq_shift,
      // daq_alphabetic_input  = (xflags_t) 4  << daq_shift,
      // daq_input_align_right = (xflags_t) 6  << daq_shift,
      // daq_input_reversed    = (xflags_t) 7  << daq_shift,
      // daq_zero_fill         = (xflags_t) 8  << daq_shift,
      // daq_space_fill        = (xflags_t) 9  << daq_shift,
      // daq_tabstop           = (xflags_t) 10 << daq_shift,

      xflags_reserverd_bit1 = (xflags_t) 1 << 29,
      xflags_reserverd_bit2 = (xflags_t) 1 << 30,
      xflags_reserverd_bit3 = (xflags_t) 1 << 31,

      qualifier_mask = decsca_protected | spa_protected | ssa_selected | daq_guarded | daq_protected,

      // 以下に \e[m でクリアされない物を列挙する。
      // SGR(9903)-SGR(9906) で提供している decdhl_mask については \e[m でクリアできる事にする。
      non_sgr_xflags_mask = is_sub_set | is_sup_set | sco_mask | qualifier_mask,
    };

    aflags_t aflags = 0;
    xflags_t xflags = 0;
    color_t  fg = 0;
    color_t  bg = 0;

    constexpr attribute_t() {}
    constexpr attribute_t(std::uint32_t value): aflags(value) {}

    constexpr bool is_default() const {
      return aflags == 0 && xflags == 0 && fg == 0 && bg == 0;
    }
    constexpr bool operator==(attribute_t const& rhs) const {
      return aflags == rhs.aflags && xflags == rhs.xflags && fg == rhs.fg && bg == rhs.bg;
    }
    constexpr bool operator!=(attribute_t const& rhs) const { return !(*this == rhs); }

  private:
    constexpr byte fg_index() const { return (aflags & fg_color_mask) >> fg_color_shift; }
    constexpr byte bg_index() const { return (aflags & bg_color_mask) >> bg_color_shift; }
  public:
    constexpr byte fg_space() const { return aflags & is_fg_indexed ? (byte) color_space_indexed : fg_index(); }
    constexpr byte bg_space() const { return aflags & is_bg_indexed ? (byte) color_space_indexed : bg_index(); }
    constexpr color_t fg_color() const { return aflags & is_fg_indexed ? fg_index() : fg; }
    constexpr color_t bg_color() const { return aflags & is_bg_indexed ? bg_index() : bg; }

    constexpr bool is_fg_default() const { return fg_space() == color_space_default; }
    constexpr bool is_bg_default() const { return bg_space() == color_space_default; }

  public:
    void set_fg(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      if (colorSpace == color_space_indexed) {
        aflags |= is_fg_indexed;
        aflags = (aflags & ~(aflags_t) fg_color_mask) | index << fg_color_shift;
        fg = 0;
      } else {
        aflags &= ~is_fg_indexed;
        aflags = (aflags & ~(aflags_t) fg_color_mask) | colorSpace << fg_color_shift;
        fg = index;
      }
    }
    void reset_fg() {
      set_fg(0, color_space_default);
    }
    void set_bg(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      if (colorSpace == color_space_indexed) {
        aflags |= is_bg_indexed;
        aflags = (aflags & ~(aflags_t) bg_color_mask) | index << bg_color_shift;
        bg = 0;
      } else {
        aflags &= ~is_bg_indexed;
        aflags = (aflags & ~(aflags_t) bg_color_mask) | colorSpace << bg_color_shift;
        bg = index;
      }
    }
    void reset_bg() {
      set_bg(0, color_space_default);
    }

    void clear() {
      this->aflags = 0;
      this->xflags = 0;
      this->fg = 0;
      this->bg = 0;
    }
    void clear_sgr() {
      aflags = 0;
      xflags &= non_sgr_xflags_mask;
      fg = 0;
      bg = 0;
    }

  public:
    constexpr bool is_decsca_protected() const {
      return xflags & attribute_t::decsca_protected;
    }
  };

  //-----------------------------------------------------------------------------

  enum termcap_constants {
    color_space_default_bit     = 1 << attribute_t::color_space_default    ,
    color_space_transparent_bit = 1 << attribute_t::color_space_transparent,
    color_space_rgb_bit         = 1 << attribute_t::color_space_rgb        ,
    color_space_cmy_bit         = 1 << attribute_t::color_space_cmy        ,
    color_space_cmyk_bit        = 1 << attribute_t::color_space_cmyk       ,
    color_space_indexed_bit     = 1 << attribute_t::color_space_indexed    ,
  };

  //-----------------------------------------------------------------------------
  //
  // SGR capabilities
  //
  // Note:
  //   さすがにどの端末も SGR 0 は対応していると仮定している。
  //   (但し、端末が SGR 自体に対応していない場合はこの限りではない。)
  //

  struct termcap_sgrflag1 {
    aflags_t bit;
    unsigned on;
    unsigned off;
  };

  struct termcap_sgrflag2 {
    aflags_t bit1;
    unsigned on1;
    aflags_t bit2;
    unsigned on2;
    unsigned off;
  };

  struct termcap_sgrideogram {
    /*?lwiki
     * @var bool is_decoration_exclusive;
     *   現在のところ ideogram decorations はどれか一つだけが有効になる様に実装することにする。
     *   (但し、将来それぞれ独立に on/off する様に変更できる余地を残すために各項目 1bit 占有している。)
     *   但し、実際の端末では複数を有効にすることができる実装もあるだろう。
     *   その場合には、一貫した動作をさせる為には必ず既に設定されている装飾を解除する必要がある。
     *   この設定項目は出力先の端末で ideogram decorations が排他的かどうかを保持する。
     */
    unsigned single_rb {60};
    unsigned double_rb {61};
    unsigned single_lt {62};
    unsigned double_lt {63};
    unsigned single_lb {66};
    unsigned double_lb {67};
    unsigned single_rt {68};
    unsigned double_rt {69};
    unsigned stress    {64};
    unsigned reset     {65};
  };

  struct termcap_sgrcolor {
    aflags_t bit;
    unsigned base;
    unsigned off;

    // 90-97, 100-107
    unsigned aixterm_color;

    // 38:5:0-255
    struct {
      unsigned      sgr         ; //!< SGR number of ISO 8613-6 color specification
      unsigned char color_spaces; //!< supported color specs specified by bits
      char          separater   ; //!< the separator character of ISO 8613-6 SGR arguments
      bool          exact       ; //!< the separator character of ISO 8613-6 SGR arguments
      unsigned      max_index   ; //!< the number of colors available through 38:5:*
    } iso8613;

    // 1,22 / 5,25 で明るさを切り替える方式
    unsigned high_intensity_on ;
    unsigned high_intensity_off;
  };

  struct termcap_sgr_type {
    typedef attribute_t _at;
    // aflags
    termcap_sgrflag2 cap_bold         { _at::is_bold_set     , 1, _at::is_faint_set           ,  2, 22 };
    termcap_sgrflag2 cap_italic       { _at::is_italic_set   , 3, _at::is_fraktur_set         , 20, 23 };
    termcap_sgrflag2 cap_underline    { _at::underline_single, 4, _at::underline_double, 21, 24 };
    termcap_sgrflag2 cap_blink        { _at::is_blink_set    , 5, _at::is_rapid_blink_set     ,  6, 25 };
    termcap_sgrflag1 cap_inverse      { _at::is_inverse_set  , 7, 27 };
    termcap_sgrflag1 cap_invisible    { _at::is_invisible_set, 8, 28 };
    termcap_sgrflag1 cap_strike       { _at::is_strike_set   , 9, 29 };

    // xflags
    termcap_sgrflag2 cap_framed       { _at::is_frame_set       , 51, _at::is_circle_set, 52, 54 };

    termcap_sgrflag1 cap_proportional { _at::is_proportional_set, 26, 50 };
    termcap_sgrflag1 cap_overline     { _at::is_overline_set    , 53, 55 };

    termcap_sgrideogram cap_ideogram;

    // colors
    termcap_sgrcolor cap_fg {
      _at::is_fg_indexed, 30, 39,  90,
      { 38, color_space_indexed_bit | color_space_rgb_bit, ascii_semicolon, false, 255 },
      0, 0
    };
    termcap_sgrcolor cap_bg {
      _at::is_bg_indexed, 40, 49, 100,
      { 48, color_space_indexed_bit | color_space_rgb_bit, ascii_semicolon, false, 255 },
      0, 0
    };

    aflags_t aflagsNotResettable { 0 };
    xflags_t xflagsNotResettable { 0 };

  public:
    void initialize();
  };

  //-----------------------------------------------------------------------------

  struct tty_writer {
    std::FILE* file;

    // 出力先端末の性質
    termcap_sgr_type const* sgrcap;
    bool termcap_bce = false;

    tty_writer(std::FILE* file, termcap_sgr_type* sgrcap): file(file), sgrcap(sgrcap) {
      m_attr.clear();
    }

  private:
    void sgr_clear() const {
      put(ascii_esc);
      put(ascii_left_bracket);
      put(ascii_m);
    }

    void sgr_put(unsigned value) {
      if (this->sgr_isOpen) {
        put(ascii_semicolon);
      } else {
        this->sgr_isOpen = true;
        put(ascii_esc);
        put(ascii_left_bracket);
      }

      if (value) put_unsigned(value);
    }

    attribute_t m_attr;
    bool sgr_isOpen;

    void update_sgrflag1(
      aflags_t aflagsNew, aflags_t aflagsOld,
      termcap_sgrflag1 const& sgrflag);

    void update_sgrflag2(
      aflags_t aflagsNew, aflags_t aflagsOld,
      termcap_sgrflag2 const& sgrflag);

    void update_ideogram_decoration(
      xflags_t xflagsNew, xflags_t xflagsOld,
      termcap_sgrideogram const& cap);

    void update_sgrcolor(
      int colorSpaceNew, color_t colorNew,
      int colorSpaceOld, color_t colorOld,
      termcap_sgrcolor const& sgrcolor);

  public:
    void apply_attr(attribute_t newAttr);

    void put_u32(char32_t c) const { contra::encoding::put_u8(c, file); }
    void put(char c) const { std::fputc(c, file); }
    void put_str(const char32_t* s) const {
      while (*s) put(*s++);
    }
    void put_unsigned(unsigned value) const {
      if (value >= 10)
        put_unsigned(value/10);
      put((byte) (ascii_0 + value % 10));
    }
    void flush() const {
      std::fflush(file);
    }

  private:
    void put_skip(curpos_t& wskip) {
      if (m_attr.is_default() && wskip <= 4) {
        while (wskip--) put(' ');
      } else {
        put(ascii_esc);
        put(ascii_left_bracket);
        put_unsigned(wskip);
        put(ascii_C);
      }
      wskip = 0;
    }
  public:
    // test implementation
    // ToDo: output encoding
    void print_screen(board_t const& b);
  };



}
}

#endif
