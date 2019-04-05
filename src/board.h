// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_BOARD_H
#define CONTRA_BOARD_H
#include <mwg/except.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>
#include "contradef.h"

namespace contra {

  enum ascii_codes {

    // control characters

    ascii_nul = 0x00, ascii_dle = 0x10, ascii_pad = 0x80, ascii_dcs  = 0x90,
    ascii_soh = 0x01, ascii_dc1 = 0x11, ascii_hop = 0x81, ascii_pu1  = 0x91,
    ascii_stk = 0x02, ascii_dc2 = 0x12, ascii_bph = 0x82, ascii_pu2  = 0x92,
    ascii_etx = 0x03, ascii_dc3 = 0x13, ascii_nbh = 0x83, ascii_sts  = 0x93,
    ascii_eot = 0x04, ascii_dc4 = 0x14, ascii_ind = 0x84, ascii_cch  = 0x94,
    ascii_enq = 0x05, ascii_nak = 0x15, ascii_nel = 0x85, ascii_mw   = 0x95,
    ascii_ack = 0x06, ascii_syn = 0x16, ascii_ssa = 0x86, ascii_spa  = 0x96,
    ascii_bel = 0x07, ascii_etb = 0x17, ascii_esa = 0x87, ascii_epa  = 0x97,
    ascii_bs  = 0x08, ascii_can = 0x18, ascii_hts = 0x88, ascii_sos  = 0x98,
    ascii_ht  = 0x09, ascii_em  = 0x19, ascii_htj = 0x89, ascii_sgci = 0x99,
    ascii_lf  = 0x0A, ascii_sub = 0x1A, ascii_vts = 0x8A, ascii_sci  = 0x9A,
    ascii_vt  = 0x0B, ascii_esc = 0x1B, ascii_pld = 0x8B, ascii_csi  = 0x9B,
    ascii_ff  = 0x0C, ascii_fs  = 0x1C, ascii_plu = 0x8C, ascii_st   = 0x9C,
    ascii_cr  = 0x0D, ascii_gs  = 0x1D, ascii_ri  = 0x8D, ascii_osc  = 0x9D,
    ascii_so  = 0x0E, ascii_rs  = 0x1E, ascii_ss2 = 0x8E, ascii_pm   = 0x9E,
    ascii_si  = 0x0F, ascii_us  = 0x1F, ascii_ss3 = 0x8F, ascii_apc  = 0x9F,

    ascii_sp   = 0x20,
    ascii_del  = 0x7F,
    ascii_nbsp = 0xA0,

    // alnum

    ascii_0 = 0x30,
    ascii_1 = 0x31,
    ascii_2 = 0x32,
    ascii_3 = 0x33,
    ascii_4 = 0x34,
    ascii_5 = 0x35,
    ascii_6 = 0x36,
    ascii_7 = 0x37,
    ascii_8 = 0x38,
    ascii_9 = 0x39,

    ascii_A = 0x41, ascii_a = 0x61,
    ascii_B = 0x42, ascii_b = 0x62,
    ascii_C = 0x43, ascii_c = 0x63,
    ascii_D = 0x44, ascii_d = 0x64,
    ascii_E = 0x45, ascii_e = 0x65,
    ascii_F = 0x46, ascii_f = 0x66,
    ascii_G = 0x47, ascii_g = 0x67,
    ascii_H = 0x48, ascii_h = 0x68,
    ascii_I = 0x49, ascii_i = 0x69,
    ascii_J = 0x4A, ascii_j = 0x6A,
    ascii_K = 0x4B, ascii_k = 0x6B,
    ascii_L = 0x4C, ascii_l = 0x6C,
    ascii_M = 0x4D, ascii_m = 0x6D,
    ascii_N = 0x4E, ascii_n = 0x6E,
    ascii_O = 0x4F, ascii_o = 0x6F,
    ascii_P = 0x50, ascii_p = 0x70,
    ascii_Q = 0x51, ascii_q = 0x71,
    ascii_R = 0x52, ascii_r = 0x72,
    ascii_S = 0x53, ascii_s = 0x73,
    ascii_T = 0x54, ascii_t = 0x74,
    ascii_U = 0x55, ascii_u = 0x75,
    ascii_V = 0x56, ascii_v = 0x76,
    ascii_W = 0x57, ascii_w = 0x77,
    ascii_X = 0x58, ascii_x = 0x78,
    ascii_Y = 0x59, ascii_y = 0x79,
    ascii_Z = 0x5A, ascii_z = 0x7A,

    // punctuations

    ascii_exclamation   = 0x21,
    ascii_double_quote  = 0x22,
    ascii_number        = 0x23,
    ascii_dollar        = 0x24,
    ascii_percent       = 0x25,
    ascii_ampersand     = 0x26,
    ascii_single_quote  = 0x27,
    ascii_left_paren    = 0x28,
    ascii_right_paren   = 0x29,
    ascii_asterisk      = 0x2A, ascii_colon     = 0x3A,
    ascii_plus          = 0x2B, ascii_semicolon = 0x3B,
    ascii_comma         = 0x2C, ascii_less      = 0x3C,
    ascii_minus         = 0x2D, ascii_equals    = 0x3D,
    ascii_dot           = 0x2E, ascii_greater   = 0x3E,
    ascii_slash         = 0x2F, ascii_question  = 0x3F,

    ascii_at            = 0x40, ascii_back_quote   = 0x60,
    ascii_left_bracket  = 0x5B, ascii_left_brace   = 0x7B,
    ascii_backslash     = 0x5C, ascii_vertical_bar = 0x7C,
    ascii_right_bracket = 0x5D, ascii_right_brace  = 0x7D,
    ascii_circumflex    = 0x5E, ascii_tilde        = 0x7E,
    ascii_underscore    = 0x5F,
  };

  const char* get_ascii_name(char32_t value);

  typedef std::uint32_t character_t;
  typedef std::uint32_t attribute_t;
  typedef attribute_t   aflags_t;
  typedef attribute_t   xflags_t;
  typedef std::int32_t  aindex_t;
  typedef std::uint32_t color_t;

  enum character_flags {
    unicode_mask       = 0x001FFFFF,
    is_acs_character   = 0x01000000, // not yet supported
    is_wide_extension  = 0x02000000,
    is_unicode_cluster = 0x04000000, // not yet supported
    is_embedded_object = 0x08000000, // not yet supported
  };

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
     * - @const color_spec_default
     * - @const color_spec_transparent
     * - @const color_spec_rgb
     * - @const color_spec_cmy
     * - @const color_spec_cmyk
     * - @const color_spec_indexed
     */
    fg_color_mask           = 0x00FF,
    bg_color_mask           = 0xFF00,
    fg_color_shift          = 0,
    bg_color_shift          = 8,
    color_spec_default     = 0,
    color_spec_transparent = 1,
    color_spec_rgb         = 2,
    color_spec_cmy         = 3,
    color_spec_cmyk        = 4,
    color_spec_indexed     = 5,
    color_spec_default_bit     = 1 << color_spec_default    ,
    color_spec_transparent_bit = 1 << color_spec_transparent,
    color_spec_rgb_bit         = 1 << color_spec_rgb        ,
    color_spec_cmy_bit         = 1 << color_spec_cmy        ,
    color_spec_cmyk_bit        = 1 << color_spec_cmyk       ,
    color_spec_indexed_bit     = 1 << color_spec_indexed    ,

    is_fg_color_set         = (std::uint32_t) 1 << 16,
    is_bg_color_set         = (std::uint32_t) 1 << 17,
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

    non_sgr_xflags_mask = is_sub_set | is_sup_set | decdhl_mask | sco_mask,
  };

  struct extended_attribute {
    aflags_t aflags;
    xflags_t xflags;
    color_t  fg;
    color_t  bg;

  private:
    static aflags_t construct_aflags(attribute_t attr, int fgColorSpace, int bgColorSpace) {
      return (attr & ~(attribute_t) (has_extended_attribute | fg_color_mask | bg_color_mask))
        | fgColorSpace << fg_color_shift
        | bgColorSpace << bg_color_shift;
    }

  public:
    void load(attribute_t attr) {
      this->xflags = 0;

      int fgColorSpace, bgColorSpace;
      if (attr & is_fg_color_set) {
        this->fg = (attr & fg_color_mask) >> fg_color_shift;
        fgColorSpace = color_spec_indexed;
      } else {
        this->fg = 0;
        fgColorSpace = color_spec_default;
      }
      if (attr & is_bg_color_set) {
        this->bg = (attr & bg_color_mask) >> bg_color_shift;
        bgColorSpace = color_spec_indexed;
      } else {
        this->bg = 0;
        bgColorSpace = color_spec_default;
      }

      this->aflags = construct_aflags(attr, fgColorSpace, bgColorSpace);
    }

    void clear() {
      this->aflags = 0;
      this->xflags = 0;
      this->fg = 0;
      this->bg = 0;
    }

    color_t fg_space() const {return (aflags & fg_color_mask) >> fg_color_shift;}
    color_t bg_space() const {return (aflags & bg_color_mask) >> bg_color_shift;}

  public:
    extended_attribute() {}
    extended_attribute(attribute_t attr) {this->load(attr);}

  public:
    bool try_get_scalar_attribute(attribute_t& _attr) {
      if (xflags) return false;

      attribute_t attr = aflags & ~(attribute_t) (fg_color_mask | bg_color_mask | has_extended_attribute);
      if (aflags & is_fg_color_set) {
        if ((aflags & fg_color_mask) >> fg_color_shift != color_spec_indexed || fg >= 256) return false;
        attr |= fg << fg_color_shift & fg_color_mask;
      }

      if (aflags & is_bg_color_set) {
        if ((aflags & bg_color_mask) >> bg_color_shift != color_spec_indexed || bg >= 256) return false;
        attr |= bg << bg_color_shift & bg_color_mask;
      }

      _attr = attr;
      return true;
    }
  };

  template<typename Scal, typename Ex, Scal IsExtended = has_extended_attribute>
  struct extension_store {
    typedef Scal basic_type;
    typedef Ex   extended_type;
    static constexpr basic_type is_extended = IsExtended;

    typedef Scal index_type;
    static constexpr index_type invalid_index = is_extended;

    struct holder_type {
      extended_type value;
      union {
        /*?lwiki
         * @var index_type reference_count;
         * @var index_type next;
         * この holder_type 構造体が実際に使用されている時には
         * `reference_count` が参照カウントとして使用される。
         * 後の使用ために free list に繋がっている時は、
         * 次の使用されていない項目への参照として `next` が使用される。
         * 次の項目は親配列 (`extension_store::m_xattrs`) 中の番号で表現される。
         *
         */
        index_type reference_count;
        index_type next;
      };

      holder_type(): reference_count(0) {}
      holder_type(basic_type attr):
        value(attr), reference_count(0) {}
    };

    std::vector<holder_type> m_xattrs;
    index_type m_xattr_free {invalid_index};
    index_type m_count {0};

    index_type get_count(basic_type attr) const{
      if (attr & is_extended) {
        return m_xattrs[attr & ~(basic_type)is_extended].reference_count;
      } else
        return 0;
    }

    extended_attribute const* get(basic_type attr) const{
      if (attr & is_extended) {
        return &m_xattrs[attr & ~(basic_type)is_extended].value;
      } else
        return nullptr;
    }

    void inc(basic_type attr) {
      if (attr & is_extended)
        m_xattrs[attr & ~(basic_type) is_extended].reference_count++;
    }

    void dec(basic_type attr) {
      if (attr & is_extended) {
        index_type const index = attr & ~(basic_type) is_extended;
        holder_type& xattr = m_xattrs[index];
        if (--xattr.reference_count <= 0) {
          xattr.next = m_xattr_free;
          m_xattr_free = index;
          m_count--;
        }
      }
    }

    // ToDo: バグがあるっぽい。
    basic_type alloc() {
      if (m_xattr_free != invalid_index) {
        basic_type const ret = m_xattr_free | is_extended;
        holder_type& xattr = m_xattrs[m_xattr_free];
        m_xattr_free = xattr.next;
        xattr.reference_count = 1;
        m_count++;
        return ret;
      } else {
        std::size_t const index = m_xattrs.size();
        if (index >= invalid_index) {
          // 拡張属性の数が多すぎる (2Gi = 20億)。現実的には起こらないと思われる。
          std::fprintf(stderr, "reached the maximal number of xattr per board.\n");
          std::exit(EXIT_FAILURE);
        }

        basic_type const ref = index | is_extended;
        m_xattrs.emplace_back();
        m_xattrs[index].reference_count = 1;
        m_count++;
        return ref;
      }
    }

    basic_type alloc(extended_type const& _xattr) {
      basic_type const ret = alloc();
      extended_type& xattr = m_xattrs[ret & ~(basic_type) is_extended].value;
      xattr = _xattr;
      return ret;
    }

    basic_type alloc(basic_type attr) {
      mwg_assert(!(attr & is_extended));
      basic_type const ret = alloc();
      extended_type& xattr = m_xattrs[ret & ~(basic_type) is_extended].value;
      xattr = extended_type(attr);
      return ret;
    }
  };

  typedef int curpos_t;

  struct board;

  struct board_cell {
    character_t character {0};
    attribute_t attribute {0};
  };

  typedef std::uint32_t line_attr_t;

  enum line_flags {
    // bit 0
    is_line_used = 0x0001,

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
    is_character_path_rtol = 0x0002,
    is_character_path_ltor = 0x0004,
    character_path_mask    = is_character_path_rtol | is_character_path_ltor,

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


  enum nested_string_type {
    string_unknown           = 0,

    string_directed_ltor     = 1,
    string_directed_rtol     = 2,
    string_directed_charpath = 3, // 独自
    string_directed_end      = 4,
    string_reversed          = 5,
    string_reversed_end      = 6,

    _string_directed_minValue = 1,
    _string_directed_maxValue = 4,
    _string_reversed_maxValue = 6,

    string_aligned_left      = 10,
    string_aligned_right     = 11,
    string_aligned_centered  = 12,
    string_aligned_char      = 13,
    string_aligned_end       = 14,

    _string_aligned_minValue = 10,
    _string_aligned_maxValue = 14,
  };

  constexpr bool is_string_directed(nested_string_type value) {
    return _string_directed_minValue <= value && value <= _string_directed_maxValue;
  }
  constexpr bool is_string_directed_begin(nested_string_type value) {
    return _string_directed_minValue <= value && value < _string_directed_maxValue;
  }
  constexpr bool is_string_reversed(nested_string_type value) {
    return value == string_reversed || value == string_reversed_end;
  }
  constexpr bool is_string_reversed_begin(nested_string_type value) {
    return value == string_reversed;
  }
  constexpr bool is_string_bidi(nested_string_type value) {
    return _string_directed_minValue <= value && value <= _string_reversed_maxValue;
  }
  constexpr bool is_string_bidi_end(nested_string_type value) {
    return value == string_directed_end || value == string_reversed_end;
  }
  constexpr bool is_string_bidi_begin(nested_string_type value) {
    return is_string_bidi(value) && ! is_string_bidi_end(value);
  }
  constexpr bool is_string_aligned(nested_string_type value) {
    return _string_aligned_minValue <= value && value <= _string_aligned_maxValue;
  }
  constexpr bool is_string_aligned_begin(nested_string_type value) {
    return _string_aligned_minValue <= value && value < _string_aligned_maxValue;
  }
  constexpr bool is_string_end(nested_string_type value) {
    return is_string_bidi_end(value) || value == string_aligned_end;
  }
  constexpr bool is_string_corresponding_pair(nested_string_type beg, nested_string_type end) {
    if (end == string_directed_end)
      return beg != end && is_string_directed(beg);
    else if (end == string_reversed_end)
      return beg == string_reversed;
    else if (end == string_aligned_end)
      return beg != end && is_string_aligned(beg);
    else
      return false;
  }

  struct line_marker {
    curpos_t           position;
    nested_string_type stype;
  };

  struct nested_string {
    curpos_t begin;
    curpos_t end;
    nested_string_type stype;

    enum {npos = -1};
  };

  struct tabstop_property {
    curpos_t           m_pos;
    nested_string_type m_align;
    char32_t           m_alignChar;
  };

  struct board_line {
    line_attr_t lflags {0};

    /*?lwiki
     * @var curpos_t home;
     * @var curpos_t limit;
     *   行内の文字の表示範囲を指定する。
     *   文字は home <= x <= limit の範囲に出力される。
     *   limit も含まれる事に注意する。
     */
    curpos_t home     {-1};
    curpos_t limit    {-1};

  private:
    std::vector<line_marker> m_markers;

    /*?lwiki
     * @var std::vector<nested_string> m_strings_cached;
     * 想定: 開始点の番号の小さいものが先に来る。
     * 想定: 二つの文字列の関係は必ず、共有部分がないか、
     *   一方が他方を完全に包含するかのどちらかである。
     * 想定: 子よりも親が先に来る。
     */
    mutable std::vector<nested_string> m_strings_cached;

    mutable bool m_strings_updated {true};

  public:
    void clear_markers() {
      this->m_markers.clear();
      this->m_strings_cached.clear();
      this->m_strings_updated = true;
    }

    void update_markers_on_overwrite(curpos_t beg, curpos_t end, bool simd);
    void update_markers_on_erase(curpos_t beg, curpos_t end);

    void append_marker_at(curpos_t pos, nested_string_type marker) {
      m_strings_updated = false;
      m_markers.insert(
        std::upper_bound(
          m_markers.begin(), m_markers.end(), pos,
          [](curpos_t pos, line_marker const& marker) {return pos < marker.position;}),
        {pos, marker});
    }
    void prepend_marker_at(curpos_t pos, nested_string_type marker) {
      m_strings_updated = false;
      m_markers.insert(
        std::lower_bound(
          m_markers.begin(), m_markers.end(), pos,
          [](line_marker const& marker, curpos_t pos) {return marker.position < pos;}),
        {pos, marker});
    }
    void _set_string(curpos_t beg, curpos_t end, nested_string_type marker) {
      if (is_string_bidi(marker)) {
        append_marker_at(beg, marker);
        prepend_marker_at(end, marker == string_reversed? string_reversed_end: string_directed_end);
      } else
        mwg_check(0, "BUG: not supported");
    }

    std::vector<nested_string> const& get_nested_strings() const;
    void set_nested_strings(std::vector<nested_string>&&);
    void set_nested_strings(std::vector<nested_string> const& strings) {
      set_nested_strings(std::vector<nested_string>(strings));
    }
  public:
    std::vector<tabstop_property> tabs;

  public:
    /*?lwiki
     * @param[in] presentation_direction presentationDirection;
     *   表示部の既定の方向性を指定します。
     *   具体的には `tty_player::m_state.presentation_direction` を指定します。
     */
    bool is_rtol(presentation_direction presentationDirection) const {
      if (lflags & is_character_path_ltor)
        return false;
      else if (lflags & is_character_path_rtol)
        return true;
      else
        return is_charpath_rtol(presentationDirection);
    }
  };

  struct board_cursor {
    curpos_t    x {0};
    curpos_t    y {0};

  public:
    attribute_t        attribute   {0};
    extended_attribute xattr       {attribute};
    extended_attribute xattr_edit  {attribute};
    bool               xattr_dirty {false};

    bool is_attribute_changed() const {
      return xattr_dirty
        && (xattr_edit.aflags != xattr.aflags
          || xattr_edit.xflags != xattr.xflags
          || xattr_edit.fg != xattr.fg
          || xattr_edit.bg != xattr.bg);
    }
  };

  struct board {
    curpos_t m_width  {0};
    curpos_t m_height {0};

    board_cursor cur;

  public:
    attribute_t update_cursor_attribute() {
      if (cur.is_attribute_changed()) {
        cur.xattr = cur.xattr_edit;

        m_xattr_data.dec(cur.attribute);

        attribute_t attr;
        if (cur.xattr.try_get_scalar_attribute(attr))
          cur.attribute = attr;
        else
          cur.attribute = m_xattr_data.alloc(cur.xattr);
      }

      cur.xattr_dirty = false;
      return cur.attribute;
    }

  private:
    std::vector<board_cell> m_cells;
    curpos_t m_rotation {0};
    std::vector<int>  m_i2iline;
    std::vector<board_line> m_lines;

    std::size_t internal_line_index(int y) const {
      return m_i2iline[(m_rotation + y) % m_height];
    }

  public:
    void resize(int width, int height) {
      // ToDo: 以前のデータを移す
      m_width = width;
      m_height = height;
      m_cells.resize(width * height);
      m_lines.resize(height);
      m_i2iline.resize(height);
      for (int y = 0; y < height; y++)
        m_i2iline[y] = y;
    }

    const board_cell* cell(int x, int y) const {
      return &m_cells[internal_line_index(y) * m_width + x];
    }

    board_cell* cell(int x, int y) {
      return const_cast<board_cell*>(const_cast<board const*>(this)->cell(x, y));
    }

    const board_line* line(int y) const {
      return &m_lines[internal_line_index(y)];
    }

    board_line* line(int y) {
      return const_cast<board_line*>(const_cast<board const*>(this)->line(y));
    }

    void rotate(int offset = 1) {
      this->m_rotation = (m_rotation + offset) % m_height;
    }

  public:
    presentation_direction m_presentationDirection {presentation_direction_default};

  private:
    curpos_t line_length(curpos_t y) const {
      board_cell const* cell = this->cell(0, y);
      for (curpos_t end = m_width; end > 0; end--)
        if (cell[end - 1].character) return end;
      return 0;
    }

    curpos_t convert_position(curpos_t y, curpos_t srcX, bool toPresentationPosition) const {
      board_line const* const line = this->line(y);
      std::vector<nested_string> const& strings = line->get_nested_strings();
      if (strings.empty()) return srcX;

      bool const defaultRToL = line->is_rtol(m_presentationDirection);
      curpos_t const len = this->line_length(y);

      bool rtol = defaultRToL;
      curpos_t x = toPresentationPosition ? 0 : srcX;
      for (nested_string const& range : strings) {
        curpos_t const referenceX = toPresentationPosition ? srcX : x;

        if (referenceX < range.begin) break;

        curpos_t const end = range.end == nested_string::npos ? len : range.end;
        if (referenceX < end) {
          bool const reverse = range.stype == string_reversed
            || (rtol ? range.stype == string_directed_ltor :
              range.stype == string_directed_rtol)
            || (range.stype == string_directed_charpath && rtol != defaultRToL);

          if (reverse) {
            x = end - 1 - (x - range.begin);
            rtol = !rtol;
          }
        }
      }

      if (toPresentationPosition) {
        x = srcX - x;
        if (defaultRToL != rtol) x = -x;
      }

      return x;
    }

  public:
    curpos_t to_data_position(curpos_t y, curpos_t presentationX) const {
      return convert_position(y, presentationX, false);
    }

    curpos_t to_presentation_position(curpos_t y, curpos_t dataX) const {
      return convert_position(y, dataX, true);
    }

  public:
    curpos_t line_home(board_line const* line) const {
      return line->home < 0 ? 0 : line->home;
    }
    curpos_t line_limit(board_line const* line) const {
      return line->limit < 0 ? m_width - 1 : line->limit;
    }
    curpos_t line_home(curpos_t y) const { return line_home(line(y)); }
    curpos_t line_limit(curpos_t y) const { return line_limit(line(y)); }

  public:
    typedef extension_store<attribute_t, extended_attribute, has_extended_attribute> extended_attribute_store;
    extended_attribute_store m_xattr_data;

    void clear_cell(board_cell* cell) {
      if (cell->attribute & has_extended_attribute)
        this->m_xattr_data.dec(cell->attribute);
      cell->attribute = 0;

      // ToDo拡張glyph dec
      cell->character = 0;
    }

    void set_character(board_cell* cell, char32_t ch) {
      // ToDo 拡張glyph
      cell->character = ch & unicode_mask;
    }

    void set_attribute(board_cell* cell, attribute_t attr) {
      if (cell->attribute == attr) return;
      if (cell->attribute & has_extended_attribute)
        this->m_xattr_data.dec(cell->attribute);
      cell->attribute = attr;
      if (attr & has_extended_attribute)
        this->m_xattr_data.inc(attr);
    }

  public:
    void clear_range(board_cell* cell, board_cell* cellN) {
      for (; cell < cellN; cell++)
        clear_cell(cell);
    }
    void clear_line(int y) {
      board_cell* const cell = &this->m_cells[internal_line_index(y) * m_width];
      clear_range(cell, cell + m_width);
    }
    void clear_screen() {
      board_cell* const cell = &this->m_cells[0];
      clear_range(cell, cell + m_width * m_height);
    }
    void set_cell(board_cell* cell, char32_t ch, attribute_t attr) {
      set_character(cell, ch);
      set_attribute(cell, attr);
    }

    void put_character(board_cell* cell, char32_t ch, attribute_t attr, int charWidth) {
      curpos_t const x = (cell - &m_cells[0]) % m_width;
      curpos_t const xend = x + charWidth;
      if (xend > m_width) return;

      board_cell* const cells = cell - x;

      curpos_t x1;
      if (cell->character & is_wide_extension) {
        for (x1 = x - 1; x1 >= 0; x1--) {
          bool const isMainChar = (cells[x1].character & is_wide_extension) != 0;
          clear_cell(cells + x1);
          if (isMainChar) break;
        }
      }

      x1 = x;
      set_cell(cells + x1++, ch, attr);
      while (x1 < xend)
        set_cell(cells + x1++, is_wide_extension, 0);

      while (x1 < m_width && cells[x1].character & is_wide_extension)
        clear_cell(cells + x1++);
    }

  };
}
#endif
