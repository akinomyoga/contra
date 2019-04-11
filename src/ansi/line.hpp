// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_LINE_HPP
#define CONTRA_ANSI_LINE_HPP
#include <mwg/except.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <tuple>
#include "../contradef.h"
#include "util.hpp"

// debugging
#include <cstdio>
#include "enc.utf8.hpp"

namespace contra {
namespace ansi {

  struct character_t {
    enum flags {
      unicode_mask       = 0x001FFFFF,
      flag_acs_character   = 0x01000000, // not yet supported
      flag_embedded_object = 0x08000000, // not yet supported

      flag_wide_extension    = 0x10000000,
      flag_cluster_extension = 0x20000000,
      flag_marker            = 0x40000000,

      marker_base    = flag_marker | 0x00200000,
      marker_sds_l2r = marker_base + 1,
      marker_sds_r2l = marker_base + 2,
      marker_sds_end = marker_base + 3,
      marker_srs_beg = marker_base + 4,
      marker_srs_end = marker_base + 5,

      // Unicode bidi markers
      marker_lre = flag_marker | 0x202A,
      marker_rle = flag_marker | 0x202B,
      marker_pdf = flag_marker | 0x202C,
      marker_lro = flag_marker | 0x202D,
      marker_rlo = flag_marker | 0x202E,
      marker_lri = flag_marker | 0x2066,
      marker_rli = flag_marker | 0x2067,
      marker_fsi = flag_marker | 0x2068,
      marker_pdi = flag_marker | 0x2069,
    };

    std::uint32_t value;

    constexpr character_t(): value() {}
    constexpr character_t(std::uint32_t value): value(value) {}

    constexpr bool is_extension() const {
      return value & (flag_wide_extension | flag_cluster_extension);
    }
    constexpr bool is_wide_extension() const {
      return value & flag_wide_extension;
    }
    constexpr bool is_marker() const {
      return value & flag_marker;
    }
  };

  typedef std::uint32_t color_t;
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
      is_fg_color_set            = (std::uint32_t) 1 << 16,
      is_bg_color_set            = (std::uint32_t) 1 << 17,
      fg_color_mask              = 0x00FF,
      bg_color_mask              = 0xFF00,
      fg_color_shift             = 0,
      bg_color_shift             = 8,
      color_space_default        = 0,
      color_space_transparent    = 1,
      color_space_rgb            = 2,
      color_space_cmy            = 3,
      color_space_cmyk           = 4,
      color_space_indexed        = 5,

      is_bold_set             = (std::uint32_t) 1 << 18, // -+- SGR 1,2
      is_faint_set            = (std::uint32_t) 1 << 19, // -'
      is_italic_set           = (std::uint32_t) 1 << 20, // -+- SGR 3,20
      is_fraktur_set          = (std::uint32_t) 1 << 21, // -'
      is_underline_set        = (std::uint32_t) 1 << 22, // -+- SGR 4,21
      is_double_underline_set = (std::uint32_t) 1 << 23, // -'
      is_blink_set            = (std::uint32_t) 1 << 24, // -+- SGR 5,6
      is_rapid_blink_set      = (std::uint32_t) 1 << 25, // -'
      is_inverse_set          = (std::uint32_t) 1 << 26, // SGR 7
      is_invisible_set        = (std::uint32_t) 1 << 27, // SGR 8
      is_strike_set           = (std::uint32_t) 1 << 28, // SGR 9

      attribute_reserved_bit1 = (std::uint32_t) 1 << 29,
      attribute_reserved_bit2 = (std::uint32_t) 1 << 30,

      // only valid for attribute_t
      has_extended_attribute  = (std::uint32_t) 1 << 31,
    };

    enum extended_flags {
      // bit 0-3: SGR 10-19
      ansi_font_mask  = 0x0000000F,
      ansi_font_shift = 0,

      // bit 4,5: PLD, PLU
      is_sub_set  = (std::uint32_t) 1 << 4,
      is_sup_set  = (std::uint32_t) 1 << 5,

      // bit 6,7: DECDHL, DECDWL, DECSWL
      decdhl_mask         = (std::uint32_t) 0x3 << 6,
      decdhl_single_width = (std::uint32_t) 0x0 << 6,
      decdhl_double_width = (std::uint32_t) 0x1 << 6,
      decdhl_top_half     = (std::uint32_t) 0x2 << 6,
      decdhl_bottom_half  = (std::uint32_t) 0x3 << 6,

      // bit 8-10: SCO
      sco_shift     = 8,
      sco_mask      = (std::uint32_t) 0x7 << sco_shift,
      sco_default   = (std::uint32_t) 0x0 << sco_shift,
      sco_rotate45  = (std::uint32_t) 0x1 << sco_shift,
      sco_rotate90  = (std::uint32_t) 0x2 << sco_shift,
      sco_rotate135 = (std::uint32_t) 0x3 << sco_shift,
      sco_rotate180 = (std::uint32_t) 0x4 << sco_shift,
      sco_rotate225 = (std::uint32_t) 0x5 << sco_shift,
      sco_rotate270 = (std::uint32_t) 0x6 << sco_shift,
      sco_rotate315 = (std::uint32_t) 0x7 << sco_shift,

      // bit 11: DECSCA
      decsca_protected = (std::uint32_t) 1 << 11,

      // bit 12-15: SGR(ECMA-48:1986)
      is_frame_set        = (std::uint32_t) 1 << 12, // -+- SGR 51,52
      is_circle_set       = (std::uint32_t) 1 << 13, // -'
      is_overline_set     = (std::uint32_t) 1 << 14, // --- SGR 53
      is_proportional_set = (std::uint32_t) 1 << 15, // --- SGR 26 (deprecated)

      // bit 16-24: SGR(ECMA-48:1986) ideogram decorations
      is_ideogram_single_rb_set = (std::uint32_t) 1 << 16, // -+- SGR 60,61
      is_ideogram_double_rb_set = (std::uint32_t) 1 << 17, // -'
      is_ideogram_single_lt_set = (std::uint32_t) 1 << 18, // -+- SGR 62,63
      is_ideogram_double_lt_set = (std::uint32_t) 1 << 19, // -'
      is_ideogram_single_lb_set = (std::uint32_t) 1 << 20, // -+- SGR 66,67
      is_ideogram_double_lb_set = (std::uint32_t) 1 << 21, // -'
      is_ideogram_single_rt_set = (std::uint32_t) 1 << 22, // -+- SGR 68,69
      is_ideogram_double_rt_set = (std::uint32_t) 1 << 23, // -'
      is_ideogram_stress_set    = (std::uint32_t) 1 << 24, // --- SGR 64
      is_ideogram_decoration_mask
      = is_ideogram_single_rb_set | is_ideogram_double_rb_set
      | is_ideogram_single_lt_set | is_ideogram_double_lt_set
      | is_ideogram_single_lb_set | is_ideogram_double_lb_set
      | is_ideogram_single_rt_set | is_ideogram_double_rt_set
      | is_ideogram_stress_set,

      non_sgr_xflags_mask = is_sub_set | is_sup_set | decdhl_mask | sco_mask | decsca_protected,
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

  public:
    constexpr byte fg_index() const { return (aflags & fg_color_mask) >> fg_color_shift; }
    constexpr byte bg_index() const { return (aflags & bg_color_mask) >> bg_color_shift; }
    constexpr byte fg_space() const { return aflags & is_fg_color_set ? (byte) color_space_indexed : fg_index(); }
    constexpr byte bg_space() const { return aflags & is_bg_color_set ? (byte) color_space_indexed : bg_index(); }
    constexpr byte fg_color() const { return aflags & is_fg_color_set ? fg_index() : fg; }
    constexpr byte bg_color() const { return aflags & is_bg_color_set ? bg_index() : bg; }

  public:
    void set_fg(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      if (colorSpace == color_space_indexed) {
        aflags |= is_fg_color_set;
        aflags = (aflags & ~fg_color_mask) | index << fg_color_shift;
        fg = 0;
      } else {
        aflags &= ~is_fg_color_set;
        aflags = (aflags & ~fg_color_mask) | colorSpace << fg_color_shift;
        fg = index;
      }
    }
    void reset_fg() {
      set_fg(0, color_space_default);
    }
    void set_bg(color_t index, std::uint32_t colorSpace = color_space_indexed) {
      if (colorSpace == color_space_indexed) {
        aflags |= is_bg_color_set;
        aflags = (aflags & ~bg_color_mask) | index << bg_color_shift;
        bg = 0;
      } else {
        aflags &= ~is_bg_color_set;
        aflags = (aflags & ~bg_color_mask) | colorSpace << bg_color_shift;
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
  };

  struct cell_t {
    character_t   character;
    attribute_t   attribute;
    std::uint32_t width;

    cell_t() {}
    constexpr cell_t(std::uint32_t c):
      character(c), attribute(0),
      width(character.is_extension() || character.is_marker() ? 0 : 1) {}

    bool is_zero_width_body() const {
      return width == 0 && !character.is_extension();
    }
  };

  typedef std::int32_t curpos_t;

  struct line_attr_t: contra::util::flags<std::uint32_t, line_attr_t> {
    using base = contra::util::flags<std::uint32_t, line_attr_t>;
    using def = base::def;

    template<typename... Args>
    line_attr_t(Args&&... args): base(std::forward<Args>(args)...) {}

    static constexpr def none = 0;

    // bit 0
    static constexpr def is_line_used = 0x0001;

    // bit 1-2
    /*?lwiki
     * @const is_character_path_rtol
     * If this flag is set, the character path of the line
     * is right-to-left in the case of a horizontal line
     * or bottom-top-top in the case of a vertical line.
     * @const is_character_path_ltor
     * If this flag is set, the character path of the line
     * is left-to-right in the case of a horizontal line
     * or top-to-bottom in the case of a vertical line.
     *
     * If neither bits are set, the default character path
     * defined by SPD is used.
     */
    static constexpr def is_character_path_rtol = 0x0002;
    static constexpr def is_character_path_ltor = 0x0004;
    static constexpr def character_path_mask    = is_character_path_rtol | is_character_path_ltor;

    // bit 6-7: DECDHL, DECDWL, DECSWL
    // The same values are used as in `xflags_t`.
    // The related constants are defined in `enum extended_flags`.
  };

  enum presentation_direction {
    presentation_direction_default = 0,
    presentation_direction_lrtb = 0,
    presentation_direction_tbrl = 1,
    presentation_direction_tblr = 2,
    presentation_direction_rltb = 3,
    presentation_direction_btlr = 4,
    presentation_direction_rlbt = 5,
    presentation_direction_lrbt = 6,
    presentation_direction_btrl = 7,
  };

  constexpr bool is_charpath_rtol(presentation_direction presentationDirection) {
    return presentationDirection == presentation_direction_rltb
      || presentationDirection == presentation_direction_btlr
      || presentationDirection == presentation_direction_rlbt
      || presentationDirection == presentation_direction_btrl;
  }

  enum line_segment_flags {
    line_segment_slice,
    line_segment_fill,
    line_segment_space,
  };

  struct line_segment_t {
    curpos_t p1;
    curpos_t p2;
    int type;
  };

  struct line_t {
    std::vector<cell_t> m_cells;
    line_attr_t m_lflags = {0};
    curpos_t m_home  {-1};
    curpos_t m_limit {-1};

    // !m_prop_enabled の時、データ位置と m_cells のインデックスは一致する。
    //   全角文字は wide_extension 文字を後ろにつける事により調節する。
    //   Grapheme cluster やマーカーは含まれない。
    // m_prop_enabled の時、データ位置と m_cells のインデックスは一致しない。
    //   各セルの幅を加算して求める必要がある。
    //   ゼロ幅の Grapheme cluster やマーカー等を含む事が可能である。
    bool m_prop_enabled = false;

    // !m_monospace の時に前回の書き込み位置の情報をキャッシュしておく。
    mutable std::size_t m_prop_i;
    mutable curpos_t m_prop_x;

    std::uint32_t m_version = 0;

    struct nested_string {
      curpos_t begin;
      curpos_t end;
      std::uint32_t beg_marker;
      std::uint32_t end_marker;
      bool r2l;
      int parent;
    };
    mutable std::vector<nested_string> m_strings_cache;
    mutable bool m_strings_r2l = false;
    mutable std::uint32_t m_strings_version = (std::uint32_t) -1;

  public:
    std::vector<cell_t> const& cells() const { return m_cells; }

  private:
    void _initialize_content(curpos_t width, attribute_t const& attr) {
      if (attr.is_default()) return;
      cell_t fill;
      fill.character = ascii_nul;
      fill.attribute = attr;
      fill.width = 1;
      this->m_cells.resize(width, fill);
    }

  public:
    void clear() {
      this->clear_content();
      m_lflags = (line_attr_t) 0;
      m_home = -1;
      m_limit = -1;
    }
    void clear(curpos_t width, attribute_t const& attr) {
      this->clear();
      this->_initialize_content(width, attr);
    }

    void clear_content() {
      this->m_cells.clear();
      m_prop_enabled = false;
      m_prop_i = 0;
      m_prop_x = 0;
      m_strings_cache.clear();
      m_strings_version = (std::uint32_t) -1;
      m_strings_r2l = false;
      m_version++;
    }
    void clear_content(curpos_t width, attribute_t const& attr) {
      this->clear_content();
      this->_initialize_content(width, attr);
    }

  private:
    void convert_to_proportional() {
      if (this->m_prop_enabled) return;
      this->m_prop_enabled = true;
      this->m_version++;
      m_cells.erase(
        std::remove_if(m_cells.begin(), m_cells.end(),
          [] (cell_t const& cell) { return cell.character.is_wide_extension(); }),
        m_cells.end());
      m_prop_i = 0;
      m_prop_x = 0;
    }

  private:
    void _mono_generic_replace_cells(curpos_t xL, curpos_t xR, cell_t const* cell, int count, int repeat, curpos_t width, int implicit_move);
    void _mono_write_cells(curpos_t x, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      bool flag_zw = false;
      for (int i = 0; i < count; i++) {
        curpos_t const w = cell[i].width;
        if (w == 0) flag_zw = true;
        width += w;
      }
      width *= repeat;

      if (flag_zw) {
        convert_to_proportional();
        _prop_generic_replace_cells(x, x + width, cell, count, repeat, width, implicit_move);
      } else
        _mono_generic_replace_cells(x, x + width, cell, count, repeat, width, implicit_move);
    }
    void _mono_replace_cells(curpos_t x1, curpos_t x2, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      bool flag_zw = false;
      for (int i = 0; i < count; i++) {
        curpos_t const w = cell[i].width;
        if (w == 0) flag_zw = true;
        width += w;
      }
      width *= repeat;

      if (flag_zw) {
        convert_to_proportional();
        _prop_generic_replace_cells(x1, x2, cell, count, repeat, width, implicit_move);
      } else
        _mono_generic_replace_cells(x1, x2, cell, count, repeat, width, implicit_move);
    }
    void _mono_delete_cells(curpos_t x1, curpos_t x2) {
      _mono_generic_replace_cells(x1, x2, nullptr, 0, 0, 0, 0);
    }

  private:
    std::pair<std::size_t, curpos_t> _prop_glb(curpos_t xdst, bool include_zw_body) const {
      std::size_t const ncell = m_cells.size();
      std::size_t i = 0;
      curpos_t x = 0;
      if (m_prop_x <= xdst) {
        i = m_prop_i;
        x = m_prop_x;
      }
      while (i < ncell) {
        curpos_t xnew = x + m_cells[i].width;
        if (xdst < xnew) break;
        ++i;
        x = xnew;
      }
      if (include_zw_body && x == xdst)
        while (i > 0 && m_cells[i - 1].is_zero_width_body()) i--;
      m_prop_i = i;
      m_prop_x = x;
      return {i, x};
    }
    std::pair<std::size_t, curpos_t> _prop_lub(curpos_t xdst, bool include_zw_body) const {
      std::size_t const ncell = m_cells.size();
      std::size_t i = 0;
      curpos_t x = 0;
      if (m_prop_x <= xdst) {
        i = m_prop_i;
        x = m_prop_x;
        if (!include_zw_body && x == xdst)
          while (i > 0 && m_cells[i - 1].is_zero_width_body()) i--;
      }
      while (i < ncell && x < xdst) x += m_cells[i++].width;
      while (i < ncell && m_cells[i].character.is_extension()) i++;
      if (include_zw_body && x == xdst)
        while (i < ncell && m_cells[i].is_zero_width_body()) i++;
      m_prop_i = i;
      m_prop_x = x;
      return {i, x};
    }
    void _prop_generic_replace_cells(curpos_t xL, curpos_t xR, cell_t const* cell, int count, int repeat, curpos_t width, int implicit_move);
    void _prop_write_cells(curpos_t pos, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      for (int i = 0; i < count; i++) width += cell[i].width;
      width *= repeat;
      curpos_t const left = pos, right = pos + width;
      _prop_generic_replace_cells(left, right, cell, count, repeat, width, implicit_move);
    }
    void _prop_replace_cells(curpos_t x1, curpos_t x2, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      for (int i = 0; i < count; i++) width += cell[i].width;
      width *= repeat;
      _prop_generic_replace_cells(x1, x2, cell, count, repeat, width, implicit_move);
    }
    void _prop_delete_cells(curpos_t x1, curpos_t x2) {
      _prop_generic_replace_cells(x1, x2, nullptr, 0, 0, 0, 0);
    }

  public:
    void write_cells(curpos_t x, cell_t const* cell, int count, int repeat, int implicit_move) {
      if (m_prop_enabled)
        _prop_write_cells(x, cell, count, repeat, implicit_move);
      else
        _mono_write_cells(x, cell, count, repeat, implicit_move);
    }
    void insert_cells(curpos_t x, cell_t const* cell, int count, int repeat) {
      if (m_prop_enabled)
        _prop_replace_cells(x, x, cell, count, repeat, 0);
      else
        _mono_replace_cells(x, x, cell, count, repeat, 0);
    }
    void delete_cells(curpos_t x1, curpos_t x2) {
      if (m_prop_enabled)
        _prop_delete_cells(x1, x2);
      else
        _mono_delete_cells(x1, x2);
    }
    void replace_cells(curpos_t x1, curpos_t x2, cell_t const* cell, int count, int repeat, int implicit_move) {
      if (m_prop_enabled)
        _prop_replace_cells(x1, x2, cell, count, repeat, implicit_move);
      else
        _mono_replace_cells(x1, x2, cell, count, repeat, implicit_move);
    }

  private:
    void _mono_reverse(curpos_t width) {
      if ((curpos_t) m_cells.size() != width)
        m_cells.resize(width, cell_t(ascii_nul));

      std::reverse(m_cells.begin(), m_cells.end());
      for (auto cell = m_cells.begin(), cellM = m_cells.end() - 1; cell < cellM; ++cell) {
        auto beg = cell;
        while (cell < cellM && cell->character.is_wide_extension()) ++cell;
        if (cell != beg) std::iter_swap(beg, cell);
      }

      m_version++;
    }
    void _prop_reverse(curpos_t width);

  public:
    void reverse(curpos_t width) {
      if (m_prop_enabled)
        _prop_reverse(width);
      else
        _mono_reverse(width);
    }

  public:
    /// @fn cell_t const& char_at(curpos_t x) const;
    /// 指定した位置にある有限幅の文字を取得します。
    /// @param[in] x データ位置を指定します。
    character_t char_at(curpos_t x) const {
      if (!m_prop_enabled) {
        if (x < 0 || (std::size_t) x >= m_cells.size()) return ascii_nul;
        while (x > 0 && m_cells[x].character.is_extension()) x--;
        return x;
      } else {
        std::size_t i;
        curpos_t _;
        std::tie(i, _) = _prop_glb(x, false);
        return i < m_cells.size() ? m_cells[i].character : ascii_nul;
      }
    }

  public:
    bool is_r2l(presentation_direction board_charpath) const {
      if (m_lflags & line_attr_t::is_character_path_ltor)
        return false;
      else if (m_lflags & line_attr_t::is_character_path_rtol)
        return true;
      else
        return is_charpath_rtol(board_charpath);
    }

    std::vector<nested_string> const& update_strings(curpos_t width, bool line_r2l) const;

  private:
    curpos_t convert_position(bool toPresentationPosition, curpos_t srcX, curpos_t width, bool line_r2l) const {
      if (!m_prop_enabled) return line_r2l ? std::max(0, width - srcX - 1) : srcX;
      std::vector<nested_string> const& strings = this->update_strings(width, line_r2l);
      if (strings.empty()) return line_r2l ? std::max(0, width - srcX - 1) : srcX;

      int r2l = 0;
      curpos_t x = toPresentationPosition ? 0 : srcX;
      for (nested_string const& range : strings) {
        curpos_t const referenceX = toPresentationPosition ? srcX : x;
        if (referenceX < range.begin) break;
        if (referenceX < range.end && range.r2l != r2l) {
          x = range.end - 1 - (x - range.begin);
          r2l = range.r2l;
        }
      }

      if (toPresentationPosition) {
        x = srcX - x;
        if (strings[0].r2l != r2l) x = -x;
      }

      return x;
    }

  private:
    curpos_t _prop_to_presentation_position(curpos_t x, curpos_t width, bool line_r2l) const;
  public:
    curpos_t to_data_position(curpos_t x, curpos_t width, presentation_direction board_charpath) const {
      bool const line_r2l = is_r2l(board_charpath);
      // !m_prop_enabled の時は SDS/SRS も Unicode bidi も存在しない。
      if (!m_prop_enabled) return line_r2l ? std::max(0, width - x - 1) : x;
      return convert_position(false, x, width, line_r2l);
    }
    curpos_t to_presentation_position(curpos_t x, curpos_t width, presentation_direction board_charpath) const {
      bool const line_r2l = is_r2l(board_charpath);
      // !m_prop_enabled の時は SDS/SRS も Unicode bidi も存在しない。
      if (!m_prop_enabled) return line_r2l ? std::max(0, width - x - 1) : x;

      // キャッシュがある時はそれを使った方が速いだろう。
      if (m_strings_version == m_version && m_strings_r2l == line_r2l)
        return convert_position(true, x, width, line_r2l);
      return _prop_to_presentation_position(x, width, line_r2l);
    }

  public:
    typedef std::vector<std::pair<curpos_t, curpos_t>> slice_ranges_t;
    void calculate_data_ranges_from_presentation_range(slice_ranges_t& ret, curpos_t x1, curpos_t x2, curpos_t width, bool line_r2l) const;

  public:
    /// @fn std::size_t find_innermost_string(curpos_t x, bool left, curpos_t width, bool line_r2l) const;
    /// 指定した位置がどの入れ子文字列の属しているかを取得します。
    /// @param[in] x 入れ子状態を調べるデータ位置を指定します。
    /// @param[in] left 入れ子状態を調べる位置が境界上の零幅文字の左側か右側かを指定します。
    ///   この引数が true の時に境界上の零幅文字の左側に於ける状態を取得します。
    /// @return line_t::update_strings の戻り値配列に於けるインデックスを返します。
    std::size_t find_innermost_string(curpos_t x, bool left, curpos_t width, bool line_r2l) const {
      if (!m_prop_enabled) return 0;
      auto const& strings = this->update_strings(width, line_r2l);
      int parent = 0;
      mwg_check(strings.size() <= std::numeric_limits<int>::max());
      for (std::size_t i = 1; i < strings.size(); i++) {
        auto const& str = strings[i];
        if (str.end < x || (!left && str.end == x)) continue;
        while (str.parent < parent) {
          if (x < strings[parent].end || (left && x == strings[parent].end)) return parent;
          parent = strings[parent].parent;
        }
        if (x < str.begin || (left && x == str.begin)) return parent;
        parent = i;
      }
      while (parent) {
        if (x < strings[parent].end || (left && x == strings[parent].end)) return parent;
        parent = strings[parent].parent;
      }
      return parent;
    }

  public:
    /// @fn void compose_segments(line_segment_t const* comp, int count, curpos_t width, bool line_r2l, attribute_t const& fill_attr);
    /// 表示部に於ける範囲を組み合わせて新しく行の内容を再構築します。
    void compose_segments(line_segment_t const* comp, int count, curpos_t width, bool line_r2l, attribute_t const& fill_attr);

  public:
    void ech(curpos_t p1, curpos_t p2, curpos_t width, presentation_direction board_charpath, attribute_t const& fill_attr, bool in_presentation) {
      p1 = contra::clamp(p1, 0, width);
      p2 = contra::clamp(p2, 0, width);
      if (!(p1 < p2)) return;

      bool const line_r2l = is_r2l(board_charpath);
      if (!in_presentation || !m_prop_enabled) {
        if (in_presentation && line_r2l) {
          p1 = width - p1;
          p2 = width - p2;
          std::swap(p1, p2);
        }
        cell_t fill;
        fill.character = ascii_nul;
        fill.attribute = fill_attr;
        fill.width = 1;
        write_cells(p1, &fill, 1, p2 - p1, 0);
        return;
      }

      line_segment_t comp[3] = {
        {0, p1, line_segment_slice},
        {p1, p2, line_segment_fill},
        {p2, width, line_segment_slice},
      };
      compose_segments(comp, std::size(comp), width, line_r2l, fill_attr);
    }
    void ich(curpos_t p0, curpos_t shift, curpos_t width, presentation_direction board_charpath, attribute_t const& fill_attr, bool in_presentation) {
      p0 = contra::clamp(p0, 0, width);
      bool const line_r2l = is_r2l(board_charpath);
      if (!in_presentation || !m_prop_enabled) {
        if (in_presentation && line_r2l) {
          p0 = width - p0;
          shift = -shift;
        }

        cell_t fill;
        fill.character = ascii_nul;
        fill.attribute = fill_attr;
        fill.width = 1;
        if (shift > 0) {
          curpos_t const delta = std::min(width - p0, shift);
          if (delta) {
            delete_cells(width - delta, width);
            insert_cells(p0, &fill, 1, delta);
          }
        } else if (shift < 0) {
          curpos_t const delta = std::min(p0, -shift);
          if (delta) {
            insert_cells(p0, &fill, 1, delta);
            delete_cells(0, delta);
          }
        }
        return;
      }

      if (shift > 0) {
        curpos_t const delta = std::min(width - p0, shift);
        line_segment_t comp[3] = {
          {0, p0, line_segment_slice},
          {0, delta, line_segment_fill},
          {p0, width - delta, line_segment_slice},
        };
        compose_segments(comp, std::size(comp), width, line_r2l, fill_attr);
      } else if (shift < 0) {
        curpos_t const delta = std::min(p0, -shift);
        line_segment_t comp[3] = {
          {delta, p0, line_segment_slice},
          {0, delta, line_segment_fill},
          {p0, width, line_segment_slice},
        };
        compose_segments(comp, std::size(comp), width, line_r2l, fill_attr);
      }
    }
    void dch(curpos_t p0, curpos_t shift, curpos_t width, presentation_direction board_charpath, attribute_t const& fill_attr, bool in_presentation) {
      p0 = contra::clamp(p0, 0, width);
      bool const line_r2l = is_r2l(board_charpath);
      if (!in_presentation || !m_prop_enabled) {
        if (in_presentation && line_r2l) {
          p0 = width - p0;
          shift = -shift;
        }

        cell_t fill;
        fill.character = ascii_nul;
        fill.attribute = fill_attr;
        fill.width = 1;
        if (shift > 0) {
          curpos_t const delta = std::min(width - p0, shift);
          if (delta) {
            delete_cells(p0, p0 + delta);
            insert_cells(width - delta, &fill, 1, delta);
          }
        } else if (shift < 0) {
          curpos_t const delta = std::min(p0, -shift);
          if (delta) {
            delete_cells(p0 - delta, p0);
            insert_cells(0, &fill, 1, delta);
          }
        }
        return;
      }

      if (shift > 0) {
        curpos_t const delta = std::min(width - p0, shift);
        line_segment_t comp[3] = {
          {0, p0, line_segment_slice},
          {p0 + delta, width, line_segment_slice},
          {0, delta, line_segment_fill},
        };
        compose_segments(comp, std::size(comp), width, line_r2l, fill_attr);
      } else if (shift < 0) {
        curpos_t const delta = std::min(p0, -shift);
        line_segment_t comp[3] = {
          {0, delta, line_segment_fill},
          {0, p0 - delta, line_segment_slice},
          {p0, width, line_segment_slice},
        };
        compose_segments(comp, std::size(comp), width, line_r2l, fill_attr);
      }
    }

  public:
    void debug_string_nest(curpos_t width, presentation_direction board_charpath) const {
      bool const line_r2l = is_r2l(board_charpath);
      auto const& strings = this->update_strings(width, line_r2l);

      curpos_t x = 0;
      int parent = 0;
      std::fprintf(stderr, "[");
      for (std::size_t i = 1; i < strings.size(); i++) {
        auto const& range = strings[i];

        while (range.parent < parent) {
          int delta = strings[parent].end - x;
          if (delta) std::fprintf(stderr, "%d", delta);
          std::fprintf(stderr, "]"); // 抜ける
          x = strings[parent].end;
          parent = strings[parent].parent;
        }

        if (x < range.begin) {
          std::fprintf(stderr, "%d", range.begin - x);
          x = range.begin;
        }
        std::fprintf(stderr, "[");
        parent = i;
      }
      while (parent >= 0) {
        int delta = strings[parent].end - x;
        if (delta) std::fprintf(stderr, "%d", delta);
        std::fprintf(stderr, "]"); // 抜ける
        x = strings[parent].end;
        parent = strings[parent].parent;
      }
      std::putc('\n', stderr);
    }

  public:
    void debug_dump() const {
      for (cell_t const& cell : m_cells) {
        std::uint32_t code = cell.character.value;
        if (cell.character.is_marker()) {
          switch (code) {
          case character_t::marker_sds_l2r: std::fprintf(stderr, "\x1b[91mSDS(1)\x1b[m"); break;
          case character_t::marker_sds_r2l: std::fprintf(stderr, "\x1b[91mSDS(2)\x1b[m"); break;
          case character_t::marker_srs_beg: std::fprintf(stderr, "\x1b[91mSRS(1)\x1b[m"); break;
          case character_t::marker_sds_end: std::fprintf(stderr, "\x1b[91mSDS(0)\x1b[m"); break;
          case character_t::marker_srs_end: std::fprintf(stderr, "\x1b[91mSRS(1)\x1b[m"); break;
          default: break;
          }
        } else if (code == ascii_nul) {
          std::fprintf(stderr, "\x1b[37m@\x1b[m");
        } else {
          std::putc((char) code, stderr);
        }
      }
      std::putc('\n', stderr);
    }
  };

  struct cursor_t {
    curpos_t x = 0, y = 0;
    attribute_t attribute;
  };

  struct board_t {
    contra::util::ring_buffer<line_t> m_lines;

    cursor_t cur;

    curpos_t m_width;
    curpos_t m_height;

    presentation_direction m_presentation_direction {presentation_direction_default};

    board_t(curpos_t width, curpos_t height): m_lines(height), m_width(width), m_height(height) {
      mwg_check(width > 0 && height > 0, "width = %d, height = %d", (int) width, (int) height);
      this->cur.x = 0;
      this->cur.y = 0;
    }

  public:
    line_t& line() { return m_lines[cur.y]; }
    line_t const& line() const { return m_lines[cur.y]; }
    curpos_t x() const { return cur.x; }
    curpos_t y() const { return cur.y; }

    curpos_t line_home() const {
      curpos_t const home = line().m_home;
      return home < 0 ? 0 : std::min(home, m_width - 1);
    }
    curpos_t line_limit() const {
      curpos_t const limit = line().m_limit;
      return limit < 0 ? m_width - 1 : std::min(limit, m_width - 1);
    }

    curpos_t to_data_position(curpos_t y, curpos_t x) const {
      return m_lines[y].to_data_position(x, m_width, m_presentation_direction);
    }
    curpos_t to_presentation_position(curpos_t y, curpos_t x) const {
      return m_lines[y].to_presentation_position(x, m_width, m_presentation_direction);
    }

  private:
    void initialize_lines(curpos_t y1, curpos_t y2) {
      for (curpos_t y = y1; y < y2; y++)
        m_lines[y].clear(m_width, cur.attribute);
    }

  public:
    void rotate(curpos_t count) {
      if (count > 0) {
        if (count > m_height) count = m_height;
        // ToDo: 消える行を何処かに記録する?
        initialize_lines(0, count);
        m_lines.rotate(count);
      } else if (count < 0) {
        count = -count;
        if (count < m_height) count = m_height;
        m_lines.rotate(m_lines.size() - count);
        initialize_lines(0, count);
      }
    }
    /// @fn void insert_lines(curpos_t y, curpos_t count);
    /// @param[in] y 挿入する位置を指定します。
    /// @param[in] count
    ///   挿入する行数を指定します。
    ///   負の数を指定した時、上方向にシフト・挿入します。
    void insert_lines(curpos_t y, curpos_t count) {
      if (count > 0) {
        count = std::min(count, m_height - y);
        if (!count) return;
        std::move_backward(m_lines.begin() + y, m_lines.end() - count, m_lines.end());
        initialize_lines(y, y + count);
      } else if (count < 0) {
        count = std::min(-count, y + 1);
        if (!count) return;
        std::move(m_lines.begin() + count, m_lines.begin() + y + 1, m_lines.begin());
        initialize_lines(y + 1 - count, y + 1);
      }
    }
    void delete_lines(curpos_t y, curpos_t count) {
      if (count > 0) {
        count = std::min(count, m_height - y);
        if (!count) return;
        std::move(m_lines.begin() + y + count, m_lines.end(), m_lines.begin() + y);
        initialize_lines(m_height - count, m_height);
      } else if (count < 0) {
        count = std::min(-count, y + 1);
        if (!count) return;
        std::move_backward(m_lines.begin(), m_lines.begin() + y + 1 - count, m_lines.begin() + y + 1);
        initialize_lines(0, count);
      }
    }
    void clear_screen() {
      // ToDo: 何処かに記録する
      for (auto& line : m_lines) line.clear();
    }

  public:
    void debug_print(std::FILE* file) {
      for (auto const& line : m_lines) {
        if (line.m_lflags & line_attr_t::is_line_used) {
          for (auto const& cell : line.cells()) {
            if (cell.character.is_wide_extension()) continue;
            char32_t c = (char32_t) (cell.character.value & character_t::unicode_mask);
            if (c == 0) c = '~';
            contra::encoding::put_u8(c, file);
          }
          std::putc('\n', file);
        }
      }
    }
  };

}
}

#endif
