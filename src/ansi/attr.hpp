// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_ansi_attr_hpp
#define contra_ansi_attr_hpp
#include <cstdint>

namespace contra {
namespace ansi {

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
       * Bits 0-3, 4-7 and 9-13 of `aflags_t`
       * specify foreground / background / decoration color, respectively.
       * The following constants can be used to extract/store
       * the corresponding bits of the foreground / background / decoration colors.
       * - @const fg_color_mask, bg_color_mask, dc_color_mask
       * - @const fg_color_shift, bg_color_shift, dc_color_shift
       *
       * These values hold the color space identifiers.
       * This is the list of color space identifiers:
       * - @const color_space_default
       * - @const color_space_transparent
       * - @const color_space_rgb
       * - @const color_space_cmy
       * - @const color_space_cmyk
       * - @const color_space_indexed
       */
      fg_color_mask            = 0x000F,
      bg_color_mask            = 0x00F0,
      dc_color_mask            = 0x0F00,
      fg_color_shift           = 0,
      bg_color_shift           = 4,
      dc_color_shift           = 8,
      color_space_default      = 0,
      color_space_transparent  = 1,
      color_space_rgb          = 2,
      color_space_cmy          = 3,
      color_space_cmyk         = 4,
      color_space_indexed      = 5,

      // bit 12-15: SGR 10-19
      ansi_font_mask  = 0x0000F000,
      ansi_font_shift = 12,

      // bit 16,17 is not used now

      aflags_weight_mask      = (aflags_t) 3 << 18, // bit 18-19
      is_bold_set             = (aflags_t) 1 << 18, // -+- SGR 1,2
      is_faint_set            = (aflags_t) 2 << 18, //  |
      is_heavy_set            = (aflags_t) 3 << 18, // -' (contra拡張)

      aflags_shape_mask       = (aflags_t) 3 << 20, // bit 19-20
      is_italic_set           = (aflags_t) 1 << 20, // -+- SGR 3,20
      is_fraktur_set          = (aflags_t) 2 << 20, // -'

      aflags_underline_mask   = (aflags_t) 7 << 22, // bit 22-24
      underline_single        = (aflags_t) 1 << 22, // -+- SGR 4,21
      underline_double        = (aflags_t) 2 << 22, //  |  SGR 21
      underline_curly         = (aflags_t) 3 << 22, //  |  (kitty拡張)
      underline_dotted        = (aflags_t) 4 << 22, //  |  (mintty拡張)
      underline_dashed        = (aflags_t) 5 << 22, // -'  (mintty拡張)

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
      // bit 1-3: PLD, PLU
      xflags_subsup_mask = (xflags_t) 3 << 0, // PLD,PLU
      is_sub_set         = (xflags_t) 1 << 0, //   反対の PLD/PLU でクリア
      is_sup_set         = (xflags_t) 2 << 0, //
      mintty_subsup_mask = (xflags_t) 3 << 2, // mintty SGR 73,74
      mintty_sup         = (xflags_t) 1 << 2, //   SGR(0) でクリア
      mintty_sub         = (xflags_t) 2 << 2, //

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
      non_sgr_xflags_mask = xflags_subsup_mask | sco_mask | qualifier_mask,
    };

  public:
    aflags_t aflags = 0;
    xflags_t xflags = 0;
  private:
    color_t fg = 0; // foreground color
    color_t bg = 0; // background color
    color_t dc = 0; // decoration color

  public:
    constexpr attribute_t() {}
    constexpr attribute_t(std::uint32_t value): aflags(value) {}

    constexpr bool is_default() const {
      return aflags == 0 && xflags == 0 && fg == 0 && bg == 0 && dc == 0;
    }
    constexpr bool operator==(attribute_t const& rhs) const {
      return aflags == rhs.aflags && xflags == rhs.xflags && fg == rhs.fg && bg == rhs.bg && dc == rhs.dc;
    }
    constexpr bool operator!=(attribute_t const& rhs) const { return !(*this == rhs); }

  public:
    constexpr byte fg_space() const { return (aflags & fg_color_mask) >> fg_color_shift; }
    constexpr byte bg_space() const { return (aflags & bg_color_mask) >> bg_color_shift; }
    constexpr byte dc_space() const { return (aflags & dc_color_mask) >> dc_color_shift; }
    constexpr color_t fg_color() const { return fg; }
    constexpr color_t bg_color() const { return bg; }
    constexpr color_t dc_color() const { return dc; }

    constexpr bool is_fg_default() const { return fg_space() == color_space_default; }
    constexpr bool is_bg_default() const { return bg_space() == color_space_default; }
    constexpr bool is_dc_default() const { return dc_space() == color_space_default; }

  public:
    void set_fg(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      aflags = (aflags & ~(aflags_t) fg_color_mask) | colorSpace << fg_color_shift;
      fg = index;
    }
    void set_bg(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      aflags = (aflags & ~(aflags_t) bg_color_mask) | colorSpace << bg_color_shift;
      bg = index;
    }
    void set_dc(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      aflags = (aflags & ~(aflags_t) dc_color_mask) | colorSpace << dc_color_shift;
      dc = index;
    }
    void reset_fg() { set_fg(0, color_space_default); }
    void reset_bg() { set_bg(0, color_space_default); }
    void reset_dc() { set_dc(0, color_space_default); }

    void clear() {
      this->aflags = 0;
      this->xflags = 0;
      this->fg = 0;
      this->bg = 0;
      this->dc = 0;
    }
    void clear_sgr() {
      this->aflags = 0;
      this->xflags &= non_sgr_xflags_mask;
      this->fg = 0;
      this->bg = 0;
      this->dc = 0;
    }

  public:
    constexpr bool is_decsca_protected() const {
      return xflags & attribute_t::decsca_protected;
    }
  };

}
}

#endif
