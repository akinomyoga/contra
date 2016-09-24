// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_BOARD_H
#define CONTRA_BOARD_H
#include <mwg/except.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>

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

    ascii_space = 0x20,
    ascii_del   = 0x7F,
    ascii_nbsp  = 0xA0,

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

    // delimiters

    ascii_colon     = 0x3A,
    ascii_semicolon = 0x3B,
    ascii_lbracket  = 0x5B,
  };

  typedef std::uint32_t character_t;
  typedef std::uint32_t attribute_t;
  typedef attribute_t   aflags_t;
  typedef attribute_t   xflags_t;
  typedef attribute_t   aindex_t;
  typedef std::uint32_t color_t;

  enum character_flags {
    unicode_mask       = 0x001FFFFF,
    is_acs_character   = 0x01000000,
    is_wide_extension  = 0x02000000,
    is_unicode_cluster = 0x04000000,
    is_embedded_object = 0x08000000,
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
     * these values hold the color space identifiers.
     * This is the list of color space identifiers:
     * - @const color_space_default
     * - @const color_space_transparent
     * - @const color_space_rgb
     * - @const color_space_cmy
     * - @const color_space_cmyk
     * - @const color_space_indexed
     */
    fg_color_mask           = 0x00FF,
    bg_color_mask           = 0xFF00,
    fg_color_shift          = 0,
    bg_color_shift          = 8,
    color_space_default     = 0,
    color_space_transparent = 1,
    color_space_rgb         = 2,
    color_space_cmy         = 3,
    color_space_cmyk        = 4,
    color_space_indexed     = 5,
    color_space_default_bit     = 1 << color_space_default    ,
    color_space_transparent_bit = 1 << color_space_transparent,
    color_space_rgb_bit         = 1 << color_space_rgb        ,
    color_space_cmy_bit         = 1 << color_space_cmy        ,
    color_space_cmyk_bit        = 1 << color_space_cmyk       ,
    color_space_indexed_bit     = 1 << color_space_indexed    ,

    is_fg_color_set         = 0x00010000,
    is_bg_color_set         = 0x00020000,
    is_bold_set             = 0x00040000, // -+- SGR 1,2
    is_faint_set            = 0x00080000, // -'
    is_italic_set           = 0x00100000, // -+- SGR 3,20
    is_fraktur_set          = 0x00200000, // -'
    is_underline_set        = 0x00400000, // -+- SGR 4,21
    is_double_underline_set = 0x00800000, // -'
    is_blink_set            = 0x01000000, // -+- SGR 5,6
    is_rapid_blink_set      = 0x02000000, // -'
    is_inverse_set          = 0x04000000, // SGR 7
    is_invisible_set        = 0x08000000, // SGR 8
    is_strike_set           = 0x10000000, // SGR 9

    attribute_reserved_bit1 = 0x20000000,
    attribute_reserved_bit2 = 0x40000000,

    // only valid for attribute_t
    has_extended_attribute  = 0x80000000u,
  };

  enum extended_flags {
    // bit 0-3: SGR 10-19
    ansi_font_mask = 0x0000000F,

    // bit 4,5: PLD, PLU
    is_sub_set  = 0x00000010,
    is_sup_set  = 0x00000020,

    // bit 6,7: DECDHL, DECDWL, DECSWL
    decdhl_mask         = 0x000000C0,
    decdhl_top_half     = 0x00000080,
    decdhl_bottom_half  = 0x000000C0,
    decdhl_double_width = 0x00000040,
    decdhl_single_width = 0x00000000,

    // bit 12-15:
    is_frame_set             = 0x00001000, // -+- SGR 51,52
    is_circle_set            = 0x00002000, // -'
    is_overline_set          = 0x00004000, // --- SGR 53
    is_proportional_set      = 0x00008000, // --- SGR 26 (deprecated)

    // bit 16-25: ideogram decorations
    is_ideogram_single_rb_set = 0x00010000, // -+- SGR 60,61
    is_ideogram_double_rb_set = 0x00020000, // -'
    is_ideogram_single_lt_set = 0x00040000, // -+- SGR 62,63
    is_ideogram_double_lt_set = 0x00080000, // -'
    is_ideogram_single_lb_set = 0x00100000, // -+- SGR 66,67
    is_ideogram_double_lb_set = 0x00200000, // -'
    is_ideogram_single_rt_set = 0x00400000, // -+- SGR 68,69
    is_ideogram_double_rt_set = 0x00800000, // -'
    is_ideogram_stress_set    = 0x01000000, // --- SGR 64
    is_ideogram_decoration_mask
      = is_ideogram_single_rb_set | is_ideogram_double_rb_set
      | is_ideogram_single_lt_set | is_ideogram_double_lt_set
      | is_ideogram_single_lb_set | is_ideogram_double_lb_set
      | is_ideogram_single_rt_set | is_ideogram_double_rt_set
      | is_ideogram_stress_set,

    non_sgr_xflags_mask = is_sub_set | is_sup_set | decdhl_mask,
  };

  struct extended_attribute {
    aflags_t aflags;
    xflags_t xflags;
    color_t  fg;
    color_t  bg;

  private:
    static aflags_t construct_aflags(attribute_t attr, int fgColorSpace, int bgColorSpace) {
      return (attr & ~(attribute_t)(has_extended_attribute | fg_color_mask | bg_color_mask))
        | fgColorSpace << fg_color_shift
        | bgColorSpace << bg_color_shift;
    }

  public:
    void load(attribute_t attr) {
      this->xflags = 0;

      int fgColorSpace, bgColorSpace;
      if (attr & is_fg_color_set) {
        this->fg = (attr & fg_color_mask) >> fg_color_shift;
        fgColorSpace = color_space_indexed;
      } else {
        this->fg = 0;
        fgColorSpace = color_space_default;
      }
      if (attr & is_bg_color_set) {
        this->bg = (attr & bg_color_mask) >> bg_color_shift;
        bgColorSpace = color_space_indexed;
      } else {
        this->bg = 0;
        bgColorSpace = color_space_default;
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
  };

  struct extended_attribute_store {
    struct holder_type: extended_attribute {
      union {
        /*?lwiki
         * @var aindex_t ref_count;
         * @var aindex_t next;
         * この holder_type 構造体が実際に使用されている時には
         * `ref_count` が参照カウントとして使用される。
         * 後の使用ために free list に繋がっている時は、
         * 次の使用されていない項目への参照として `next` が使用される。
         * 次の項目は親配列 (`extended_attribute_store::m_xattrs`) 中の番号で表現される。
         *
         */
        aindex_t ref_count;
        aindex_t next;
      };

      holder_type(attribute_t attr):
        extended_attribute(attr), ref_count(0) {}
    };

    std::vector<holder_type> m_xattrs;
    aindex_t m_xattr_free {0};
    aindex_t m_count {0};

    extended_attribute const* get(attribute_t attr) const{
      if (attr & has_extended_attribute) {
        return &m_xattrs[attr & ~(attribute_t)has_extended_attribute];
      } else
        return nullptr;
    }

    void inc(attribute_t attr) {
      if (attr & has_extended_attribute)
        m_xattrs[attr & ~(attribute_t) has_extended_attribute].ref_count++;
    }

    void dec(attribute_t attr) {
      if (attr & has_extended_attribute) {
        aindex_t const index = attr & ~(attribute_t) has_extended_attribute;
        holder_type& xattr = m_xattrs[index];
        if (--xattr.ref_count <= 0) {
          xattr.next = m_xattr_free;
          m_xattr_free = index;
          m_count--;
        }
      }
    }

    attribute_t alloc(attribute_t attr) {
      mwg_assert(!(attr & has_extended_attribute));

      if (m_xattr_free) {
        attribute_t const ret = m_xattr_free | has_extended_attribute;
        holder_type& xattr = m_xattrs[m_xattr_free];
        m_xattr_free = xattr.next;
        xattr.ref_count = 1;
        m_count++;
        return ret;
      } else {
        std::size_t const index = m_xattrs.size();
        if (index >= has_extended_attribute) {
          // 拡張属性の数が多すぎる (2Gi = 20億)。現実的には起こらないと思われる。
          std::fprintf(stderr, "reached the maximal number of xattr per board.\n");
          std::exit(EXIT_FAILURE);
        }

        attribute_t const ref = index | has_extended_attribute;
        m_xattrs.emplace_back(attr);
        m_xattrs[index].ref_count = 1;
        m_count++;
        return ref;
      }
    }
  };

  typedef int curpos_t;

  struct board_cell {
    character_t character {0};
    attribute_t attribute {0};
  };

  struct board_cursor {
    curpos_t    x {0};
    curpos_t    y {0};
    attribute_t attribute {0};
  };

  struct board {
    curpos_t m_width  {0};
    curpos_t m_height {0};

    board_cursor cur;

  private:
    std::vector<board_cell> m_data;
    curpos_t m_rotation {0};
    std::vector<int>  m_line_offset;

  public:
    void resize(int width, int height) {
      // ToDo: 以前のデータを移す
      m_width = width;
      m_height = height;
      m_data.resize(width * height);
      m_line_offset.resize(height);
      for (int y = 0; y < height; y++)
        m_line_offset[y] = y * width;
    }

    const board_cell& cell(int x, int y) const {
      int const iline = this->m_line_offset[(m_rotation + y) % m_height];
      return m_data[iline + x];
    }

    board_cell& cell(int x, int y) {
      return const_cast<board_cell&>(const_cast<board const*>(this)->cell(x, y));
    }

    void rotate(int offset = 1) {
      this->m_rotation = (m_rotation + offset) % m_height;
    }


    void clear_range(board_cell* cell, board_cell* cellN) {
      for (; cell < cellN; cell++) {
        if (cell->attribute & has_extended_attribute)
          this->m_xattr_data.dec(cell->attribute);
        cell->attribute = 0;

        // ToDo拡張glyph dec
        cell->character = 0;
      }
    }
  public:
    extended_attribute_store m_xattr_data;

    void clear_line(int y) {
      board_cell* cell = &this->m_data[m_line_offset[(m_rotation + y) % m_height]];
      clear_range(cell, cell + m_width);
    }
    void clear_screen() {
      board_cell* cell = &this->m_data[0];
      clear_range(cell, cell + m_width * m_height);
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
  };
}
#endif
