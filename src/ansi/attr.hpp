// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_ansi_attr_hpp
#define contra_ansi_attr_hpp
#include <cstdint>
#include "../util.hpp"

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

namespace contra::ansi {

  // attr_t, aflags_t 共通属性

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

  constexpr attr0_t attr_common_mask      = 0x3FFC0000;

  // attr_t
  constexpr attr_t attr_fg_mask  = 0x0000FF;
  constexpr attr_t attr_bg_mask  = 0x00FF00;
  constexpr int    attr_fg_shift = 0;
  constexpr int    attr_bg_shift = 8;
  constexpr attr_t attr_fg_set   = 0x010000;
  constexpr attr_t attr_bg_set   = 0x020000;
  constexpr attr_t attr_selected = 1 << 30;
  constexpr attr_t attr_extended = 1 << 31;

  // Note: color_space 2以上が attr_extended を必要とする物とする。
  constexpr int color_space_default      = 0;
  constexpr int color_space_indexed      = 1;
  constexpr int color_space_rgb          = 2;
  constexpr int color_space_cmy          = 3;
  constexpr int color_space_cmyk         = 4;
  constexpr int color_space_transparent  = 5;

  // aflags_t [cccc cccc cccc ffff --ww ssuu ubbR HS--]
  constexpr aflags_t aflags_fg_space_mask  = 0x00000F;
  constexpr aflags_t aflags_bg_space_mask  = 0x0000F0;
  constexpr aflags_t aflags_dc_space_mask  = 0x000F00;
  constexpr int      aflags_fg_space_shift = 0;
  constexpr int      aflags_bg_space_shift = 4;
  constexpr int      aflags_dc_space_shift = 8;
  constexpr aflags_t aflags_font_mask      = 0x00F000;
  constexpr int      aflags_font_shift     = 12;
  constexpr aflags_t aflags_reserved_bit1  = 1 << 16;
  constexpr aflags_t aflags_reserved_bit2  = 1 << 17;
  constexpr aflags_t aflags_reserved_bit3  = 1 << 30;
  constexpr aflags_t aflags_reserved_bit4  = 1 << 31;
  constexpr aflags_t aflags_extension_mask = 0x0000FFEE;

  // xflags_t [uudd --hh oooQ ffOP iiii rrrr rQQQ Q---]

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
  constexpr int      xflags_sco_shift     = 8;
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
  constexpr xflags_t xflags_rlogin_ideogram_mask  = 0x1F << 20;
  constexpr xflags_t xflags_rlogin_rline_mask     = 3 << 20;
  constexpr xflags_t xflags_rlogin_single_rline   = 1 << 20; // SGR 8460
  constexpr xflags_t xflags_rlogin_double_rline   = 2 << 20; // SGR 8461
  constexpr xflags_t xflags_rlogin_lline_mask     = 3 << 22;
  constexpr xflags_t xflags_rlogin_single_lline   = 1 << 22; // SGR 8462
  constexpr xflags_t xflags_rlogin_double_lline   = 2 << 22; // SGR 8463
  // bit 24: RLogin SGR(64) 二重打ち消し線
  constexpr xflags_t xflags_rlogin_double_strike  = 1 << 24; // SGR 8464

  // bit 25,26: SPA, SSA
  constexpr xflags_t xflags_spa_protected         = 1 << 25;
  constexpr xflags_t xflags_ssa_selected          = 1 << 26;
  // bit 27,28: DAQ
  constexpr xflags_t xflags_daq_guarded           = 1 << 27;
  constexpr xflags_t xflags_daq_protected         = 1 << 28;
  // constexpr int      xflags_daq_shift = 29;
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
  constexpr xflags_t xflags_reserved_bit3 = 1 << 29;
  constexpr xflags_t xflags_reserved_bit4 = 1 << 30;
  constexpr xflags_t xflags_reserved_bit5 = 1 << 31;

  constexpr xflags_t xflags_qualifier_mask = xflags_decsca_protected | xflags_spa_protected | xflags_ssa_selected | xflags_daq_guarded | xflags_daq_protected;

  // 以下に \e[m でクリアされない物を列挙する。
  // SGR(9903)-SGR(9906) で提供している decdhl_mask については \e[m でクリアできる事にする。
  constexpr xflags_t xflags_non_sgr_mask = xflags_subsup_mask | xflags_sco_mask | xflags_qualifier_mask;

}

namespace contra {
namespace ansi {

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

  struct attribute_t {
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
    constexpr byte fg_space() const { return (std::uint32_t) (aflags & aflags_fg_space_mask) >> aflags_fg_space_shift; }
    constexpr byte bg_space() const { return (std::uint32_t) (aflags & aflags_bg_space_mask) >> aflags_bg_space_shift; }
    constexpr byte dc_space() const { return (std::uint32_t) (aflags & aflags_dc_space_mask) >> aflags_dc_space_shift; }
    constexpr color_t fg_color() const { return fg; }
    constexpr color_t bg_color() const { return bg; }
    constexpr color_t dc_color() const { return dc; }

    constexpr bool is_fg_default() const { return fg_space() == color_space_default; }
    constexpr bool is_bg_default() const { return bg_space() == color_space_default; }
    constexpr bool is_dc_default() const { return dc_space() == color_space_default; }

  public:
    void set_fg(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      aflags.reset(aflags_fg_space_mask, colorSpace << aflags_fg_space_shift);
      fg = index;
    }
    void set_bg(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      aflags.reset(aflags_bg_space_mask, colorSpace << aflags_bg_space_shift);
      bg = index;
    }
    void set_dc(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      aflags.reset(aflags_dc_space_mask, colorSpace << aflags_dc_space_shift);
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
      this->xflags &= xflags_non_sgr_mask;
      this->fg = 0;
      this->bg = 0;
      this->dc = 0;
    }

  public:
    constexpr bool is_decsca_protected() const {
      return (bool) (xflags & xflags_decsca_protected);
    }
    void unselect() { xflags &= ~xflags_ssa_selected; }
    void select()   { xflags &= ~xflags_ssa_selected; }
    constexpr bool guarded() const {
      return bool(xflags & (xflags_spa_protected | xflags_daq_guarded));
    }
  };

  struct attribute_builder {
    attribute_t m_attribute;
    attribute_t const& attr() const { return m_attribute; }

  public:
    bool is_double_width() const { return (bool) (m_attribute.xflags & xflags_decdhl_mask); }

    void reset_fg() { m_attribute.reset_fg(); }
    void reset_bg() { m_attribute.reset_bg(); }
    void reset_dc() { m_attribute.reset_dc(); }
    void set_fg(color_t index, std::uint32_t colorSpace = color_space_indexed) { m_attribute.set_fg(index, colorSpace); }
    void set_bg(color_t index, std::uint32_t colorSpace = color_space_indexed) { m_attribute.set_bg(index, colorSpace); }
    void set_dc(color_t index, std::uint32_t colorSpace = color_space_indexed) { m_attribute.set_dc(index, colorSpace); }
    void clear_sgr() { m_attribute.clear_sgr(); }

    void set_weight(attr0_t weight)       { m_attribute.aflags.reset(attr_weight_mask, weight); }
    void clear_weight()                   { m_attribute.aflags &= ~attr_weight_mask; }
    void set_shape(attr0_t shape)         { m_attribute.aflags.reset(attr_shape_mask, shape); }
    void clear_shape()                    { m_attribute.aflags &= ~attr_shape_mask; }
    void set_underline(attr0_t underline) { m_attribute.aflags.reset(attr_underline_mask, underline); }
    void clear_underline()                { m_attribute.aflags &= ~attr_underline_mask; }
    void set_blink(attr0_t blink)         { m_attribute.aflags.reset(attr_blink_mask, blink); }
    void clear_blink()                    { m_attribute.aflags &= ~attr_blink_mask; }
    void set_inverse()                    { m_attribute.aflags |= attr_inverse_set; }
    void clear_inverse()                  { m_attribute.aflags &= ~attr_inverse_set; }
    void set_invisible()                  { m_attribute.aflags |= attr_invisible_set; }
    void clear_invisible()                { m_attribute.aflags &= ~attr_invisible_set; }
    void set_strike()                     { m_attribute.aflags |= attr_strike_set; }
    void clear_strike()                   { m_attribute.aflags &= ~attr_strike_set; }

    void set_font(aflags_t font)             { m_attribute.aflags.reset(aflags_font_mask, font); }
    void clear_font()                        { m_attribute.aflags &= ~aflags_font_mask; }

    void set_proportional()                  { m_attribute.xflags |= xflags_proportional_set; }
    void clear_proportional()                { m_attribute.xflags &= ~xflags_proportional_set; }
    void set_frame(xflags_t frame)           { m_attribute.xflags.reset(xflags_frame_mask, frame); }
    void clear_frame()                       { m_attribute.xflags &= ~xflags_frame_mask; }
    void set_overline()                      { m_attribute.xflags |= xflags_overline_set; }
    void clear_overline()                    { m_attribute.xflags &= ~xflags_overline_set; }
    void set_ideogram(xflags_t ideogram)     { m_attribute.xflags.reset(xflags_ideogram_mask, ideogram); }
    void clear_ideogram()                    { m_attribute.xflags &= ~xflags_ideogram_mask; }

    void set_decdhl(xflags_t decdhl)         { m_attribute.xflags.reset(xflags_decdhl_mask, decdhl); }
    void clear_decdhl()                      { m_attribute.xflags &= ~xflags_decdhl_mask; }

    void set_rlogin_rline(xflags_t ideogram) { m_attribute.xflags.reset(xflags_rlogin_rline_mask, ideogram); }
    void set_rlogin_lline(xflags_t ideogram) { m_attribute.xflags.reset(xflags_rlogin_lline_mask, ideogram); }
    void set_rlogin_double_strike()          { m_attribute.xflags |= xflags_rlogin_double_strike; }
    void clear_rlogin_ideogram()             { m_attribute.xflags &= ~xflags_rlogin_ideogram_mask; }

    void set_mintty_subsup(xflags_t subsup)  { m_attribute.xflags.reset(xflags_mintty_subsup_mask, subsup); }
    void clear_mintty_subsup()               { m_attribute.xflags &= ~xflags_mintty_subsup_mask; }

    void set_sco(xflags_t sco) { m_attribute.xflags.reset(xflags_sco_mask, sco); }
    void clear_sco()           { m_attribute.xflags &= ~xflags_sco_mask; }
    void set_decsca()          { m_attribute.xflags |= xflags_decsca_protected; }
    void clear_decsca()        { m_attribute.xflags &= ~xflags_decsca_protected; }
    void set_spa()             { m_attribute.xflags |= xflags_spa_protected; }
    void clear_spa()           { m_attribute.xflags &= ~xflags_spa_protected; }
    void set_ssa()             { m_attribute.xflags |= xflags_ssa_selected; }
    void clear_ssa()           { m_attribute.xflags &= ~xflags_ssa_selected; }
    void plu() {
      if (m_attribute.xflags & xflags_sub_set)
        m_attribute.xflags &= ~xflags_sub_set;
      else
        m_attribute.xflags |= xflags_sup_set;
    }
    void pld() {
      if (m_attribute.xflags & xflags_sup_set)
        m_attribute.xflags &= ~xflags_sup_set;
      else
        m_attribute.xflags |= xflags_sub_set;
    }
  };

  struct attribute_table {
  };

}
}

#endif