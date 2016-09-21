#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <mwg/except.h>

namespace contra {

  enum ascii {
    ascii_esc       = 0x1b,
    ascii_zero      = 0x30,
    ascii_colon     = 0x3A,
    ascii_semicolon = 0x3B,
    ascii_lbracket  = 0x5B,
    ascii_C         = 0x43,
    ascii_m         = 0x6D,
  };


  enum cell_character {
    unicode_mask       = 0x001FFFFF,
    is_acs_character   = 0x01000000,
    is_wide_extension  = 0x02000000,
    is_unicode_cluster = 0x04000000,
    is_embedded_object = 0x08000000,
  };

  typedef std::uint32_t attribute_t;
  typedef attribute_t   aflags_t;
  typedef std::uint32_t xflags_t;
  typedef std::uint32_t color_t;

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

    has_extended_attribute  = 0x80000000u, // not supported yet
  };

  static inline aflags_t attribute_to_aflags(attribute_t attr, int fgColorSpace, int bgColorSpace) {
    return (attr & ~(attribute_t)(has_extended_attribute | fg_color_mask | bg_color_mask))
      | fgColorSpace << fg_color_shift
      | bgColorSpace << bg_color_shift;
  }
  static inline color_t attribute_getfg(attribute_t attr) { return (attr & fg_color_mask) << fg_color_shift;}
  static inline color_t attribute_getbg(attribute_t attr) { return (attr & bg_color_mask) << bg_color_shift;}

  struct window_cell {
    std::uint32_t character {0};
    attribute_t attribute {0};
  };

  enum extended_flags {
    // bit 0-3: SGR 10-19
    ansi_font_mask = 0x0000000F,

    // bit 4,5: PLD, PLU
    is_sub_set = 0x00000010,
    is_sup_set = 0x00000020,

    // bit 12-15:
    is_frame_set             = 0x00001000, // -+- SGR 51,52
    is_circle_set            = 0x00002000, // -'
    is_overline_set          = 0x00004000, // --- SGR 53
    is_proportional_set      = 0x00008000, // --- SGR 26 (deprecated)

    // bit 16-25: ideographic decorations
    is_ideographic_single_rb = 0x00010000, // -+- SGR 60,61
    is_ideographic_double_rb = 0x00020000, // -'
    is_ideographic_single_lt = 0x00040000, // -+- SGR 62,63
    is_ideographic_double_lt = 0x00080000, // -'
    is_ideographic_single_lb = 0x00100000, // -+- SGR 66,67
    is_ideographic_double_lb = 0x00200000, // -'
    is_ideographic_single_rt = 0x00400000, // -+- SGR 68,69
    is_ideographic_double_rt = 0x00800000, // -'
    is_ideographic_stress    = 0x01000000, // --- SGR 64
  };

  struct extended_attribute {
    /*?lwiki
     * : @var atribute_type attribute;
     *   `attribute&fg_color` and `attribute&bg_color`
     *   are used to specify the color scheme of fg/bg
     *   instead of indices of a color table.
     *   The other flags have the same meaning as
     *   the attribute in the `struct window_cell`.
     */
    aflags_t aflags;

    /*?lwiki
     * : @var xflags_t xflags;
     */
    xflags_t xflags;

    color_t  fg;
    color_t  bg;

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

      this->aflags = attribute_to_aflags(attr, fgColorSpace, bgColorSpace);
    }

    void clear() {
      this->aflags = 0;
      this->xflags = 0;
      this->fg = 0;
      this->bg = 0;
    }

  public:
    extended_attribute() {}
    extended_attribute(attribute_t attr) {this->load(attr);}
  };

  struct extended_attribute_store {
    struct holder_type: extended_attribute {
      union {
        /*?lwiki
         * @var std::uint32_t ref_count;
         * @var std::uint32_t next;
         * 使用されている時には参照カウントとして使用される (`ref_count`)。
         * 後の仕様ために free list に繋がっている時は次の項目への参照として使用される (`next`)。
         * 次の項目は親 window の配列中の index で表現される。
         *
         */
        std::uint32_t ref_count;
        std::uint32_t next;
      };

      holder_type(attribute_t attr):
        extended_attribute(attr), ref_count(0) {}
    };

    std::vector<holder_type> m_xattrs;
    std::uint32_t m_xattr_free {0};
    std::uint32_t m_count {0};

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
        std::uint32_t const index = attr & ~(attribute_t) has_extended_attribute;
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
          std::fprintf(stderr, "reached the maximal number of xattr per window.\n");
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

  struct window_cursor {
    curpos_t    x {0};
    curpos_t    y {0};
    attribute_t attribute {0};
  };

  struct window {
    curpos_t m_width  {0};
    curpos_t m_height {0};

    // cursor
    window_cursor cur;

  private:
    std::vector<window_cell> m_data;
    int m_rotation {0};
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

    const window_cell& cell(int x, int y) const{
      int const iline = this->m_line_offset[(m_rotation + y) % m_height];
      return m_data[iline + x];
    }

    window_cell& cell(int x, int y){
      return const_cast<window_cell&>(const_cast<window const*>(this)->cell(x, y));
    }

  public:
    extended_attribute_store m_xattr_data;

    void set_character(window_cell* cell, std::uint32_t ch) {
      // ToDo 拡張glyph
      cell->character = ch;
    }

    void set_attribute(window_cell* cell, attribute_t attr) {
      if (cell->attribute & has_extended_attribute)
        this->m_xattr_data.dec(cell->attribute);
      cell->attribute = attr;
      if (attr & has_extended_attribute)
        this->m_xattr_data.inc(attr);
    }
  };

}

using namespace contra;

void put_char(window& w, std::uint32_t u) {
  window_cursor& cur = w.cur;
  int const charwidth = 1; // ToDo 文字幅
  if (cur.x + charwidth > w.m_width) {
    cur.x = 0;
    cur.y++;
    if (cur.y >= w.m_height) cur.y--; // 本当は新しい行を挿入する
  }

  if (charwidth <= 0) {
    std::exit(1); // ToDo
  }

  window_cell* c = &w.cell(cur.x, cur.y);
  w.set_character(c, u);
  w.set_attribute(c, cur.attribute);

  for (int i = 1; i < charwidth; i++) {
    w.set_character(c + i, is_wide_extension);
    w.set_attribute(c + i, 0);
  }

  cur.x += charwidth;
}

struct termcap_sgrcolor {
  aflags_t bit;
  unsigned base;
  unsigned off;

  // 90-97, 100-107
  unsigned aixterm_color;

  // 38:5:0-255
  unsigned iso8613_indexed_color;
  char iso8613_separater;

  // 1,22 で明るさを切り替える方式
  unsigned high_intensity_on;
  unsigned high_intensity_off;
};

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

struct termcap_sgr_type {
  termcap_sgrflag2 bold      {is_bold_set     , 1, is_faint_set           ,  2, 22};
  termcap_sgrflag2 italic    {is_italic_set   , 3, is_fraktur_set         , 20, 23};
  termcap_sgrflag2 underline {is_underline_set, 4, is_double_underline_set, 21, 24};
  termcap_sgrflag2 blink     {is_blink_set    , 5, is_rapid_blink_set     ,  6, 25};

  termcap_sgrflag1 inverse   {is_inverse_set  , 7, 27};
  termcap_sgrflag1 invisible {is_invisible_set, 8, 28};
  termcap_sgrflag1 strike    {is_strike_set   , 9, 29};

  // termcap_sgrflag1 pspacing    {?   , 26, 50};

  termcap_sgrcolor sgrfg {is_fg_color_set, 30, 39,  90, 38, ascii_semicolon, 0, 0};
  termcap_sgrcolor sgrbg {is_bg_color_set, 40, 49, 100, 48, ascii_semicolon, 0, 0};

  aflags_t aflagsNotResettable {0};

private:
  void initialize_aflagsNotResettable(termcap_sgrflag1 const& sgrflag) {
    if (!sgrflag.off && sgrflag.on) aflagsNotResettable |= sgrflag.bit;
  }

  void initialize_aflagsNotResettable(termcap_sgrflag2 const& sgrflag) {
    if (!sgrflag.off) {
      if (sgrflag.on1) aflagsNotResettable |= sgrflag.bit1;
      if (sgrflag.on2) aflagsNotResettable |= sgrflag.bit2;
    }
  }

  void initialize_aflagsNotResettable(termcap_sgrcolor const& sgrcolor) {
    if (!sgrcolor.off && (sgrcolor.base || sgrcolor.iso8613_indexed_color))
      aflagsNotResettable = sgrcolor.bit;
  }

public:
  void initialize() {
    aflagsNotResettable = 0;
    initialize_aflagsNotResettable(bold);
    initialize_aflagsNotResettable(italic);
    initialize_aflagsNotResettable(underline);
    initialize_aflagsNotResettable(blink);
    initialize_aflagsNotResettable(inverse);
    initialize_aflagsNotResettable(invisible);
    initialize_aflagsNotResettable(strike);
    initialize_aflagsNotResettable(sgrfg);
    initialize_aflagsNotResettable(sgrbg);
  }
};

struct tty_target {
  std::FILE* file;
  termcap_sgr_type sgrcap;

  tty_target(std::FILE* file): file(file) {
    this->sgrcap.initialize();
  }

  void put_str(const char* s) {
    while (*s) std::fputc(*s++, file);
  }
  void put_unsigned(unsigned value) {
    if (value >= 10)
      put_unsigned(value/10);
    std::fputc(ascii_zero + value % 10, file);
  }

private:
  void sa_open_sgr() {
    if (this->sa_isSgrOpen) {
      std::fputc(ascii_semicolon, file);
    } else {
      this->sa_isSgrOpen = true;
      std::fputc(ascii_esc, file);
      std::fputc(ascii_lbracket, file);
    }
  }

  attribute_t        m_attr {0};
  extended_attribute m_xattr;
  bool sa_isSgrOpen;

  void update_sgrflag1(
    aflags_t aflagsNew, aflags_t aflagsOld,
    termcap_sgrflag1 const& sgrflag
  ) {
    // when the attribute is not changed
    if (((aflagsNew ^ aflagsOld) & sgrflag.bit) == 0) return;

    // when the attribute is not supported by TERM
    if (!sgrflag.on) return;

    sa_open_sgr();
    if (aflagsNew&sgrflag.bit) {
      put_unsigned(sgrflag.on);
    } else {
      put_unsigned(sgrflag.off);
    }
  }

  void update_sgrflag2(
    aflags_t aflagsNew, aflags_t aflagsOld,
    termcap_sgrflag2 const& sgrflag
  ) {
    // when the attribute is not changed
    aflags_t bit1 = sgrflag.bit1;
    aflags_t bit2 = sgrflag.bit2;
    if (((aflagsNew ^ aflagsOld) & (bit1 | bit2)) == 0) return;

    // when the attribute is not supported by TERM
    unsigned const sgr1 = sgrflag.on1;
    unsigned const sgr2 = sgrflag.on2;
    if (!sgr1) bit1 = 0;
    if (!sgr2) bit2 = 0;
    if (!(bit1 | bit2)) return;

    aflags_t const added = aflagsNew & ~aflagsOld;
    aflags_t const removed = ~aflagsNew & aflagsOld;

    sa_open_sgr();
    if (removed & (bit1 | bit2)) {
      put_unsigned(sgrflag.off);
      if (aflagsNew & bit2) {
        std::fputc(ascii_semicolon, file);
        put_unsigned(sgr2);
      }
      if (aflagsNew & bit1) {
        std::fputc(ascii_semicolon, file);
        put_unsigned(sgr1);
      }
    } else {
      bool isOutput = false;
      if (added & bit2) {
        put_unsigned(sgr2);
        isOutput = true;
      }
      if (added & bit1) {
        if (isOutput) std::fputc(ascii_semicolon, file);
        put_unsigned(sgr1);
      }
    }
  }

  void update_sgrcolor(
    int colorSpaceNew, color_t colorNew,
    int colorSpaceOld, color_t colorOld,
    termcap_sgrcolor const& sgrcolor
  ) {
    if (colorSpaceNew == colorSpaceOld && colorNew == colorOld) return;

    switch (colorSpaceNew) {
    case color_space_default:
    case color_space_transparent: // ToDo
    case color_space_rgb: // ToDo
    case color_space_cmy: // ToDo
    case color_space_cmyk: // ToDo
    default: // fallback
      goto default_color;

    case color_space_indexed:
      if (colorNew < 8) {
        if (sgrcolor.base) {
          // e.g \e[31m
          sa_open_sgr();
          if (sgrcolor.high_intensity_off)
            put_unsigned(sgrcolor.high_intensity_off);
          put_unsigned(sgrcolor.base + colorNew);
          break;
        }
      } else if (colorNew < 16) {
        if (sgrcolor.aixterm_color) {
          // e.g. \e[91m
          sa_open_sgr();
          put_unsigned(sgrcolor.aixterm_color + (colorNew & 7));
          break;
        } else if (sgrcolor.base && sgrcolor.high_intensity_on) {
          // e.g. \e[1;31m \e[2;42m
          sa_open_sgr();
          put_unsigned(sgrcolor.high_intensity_on);
          put_unsigned(sgrcolor.base + (colorNew & 7));
          break;
        }
      }

      if (sgrcolor.iso8613_indexed_color) {
        // e.g. \e[38;5;17m
        sa_open_sgr();
        put_unsigned(sgrcolor.iso8613_indexed_color);
        std::fputc(sgrcolor.iso8613_separater, file);
        put_unsigned(5);
        std::fputc(sgrcolor.iso8613_separater, file);
        put_unsigned(colorNew);
        break;
      }

      goto default_color; // fallback

    default_color:
      sa_open_sgr();
      put_unsigned(sgrcolor.off);
      break;
    }
  }

  void ttysetattr(window const& w, attribute_t newAttr) {
    if (this->m_attr == newAttr) return;

    this->sa_isSgrOpen = false;

    if (newAttr == 0) {
      sa_open_sgr();
      this->m_attr = 0;
      this->m_xattr.clear();
    } else {
      extended_attribute _xattr;
      if (newAttr & has_extended_attribute)
        _xattr = *w.m_xattr_data.get(newAttr);
      else
        _xattr.load(newAttr);

      aflags_t const removed = ~_xattr.aflags & m_xattr.aflags;
      if (removed & sgrcap.aflagsNotResettable) {
        sa_open_sgr();
        m_xattr.aflags = 0;
      }

      update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap.bold     );
      update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap.italic   );
      update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap.underline);
      update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap.blink    );

      update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap.inverse  );
      update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap.invisible);
      update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap.strike   );

      update_sgrcolor(
        attribute_getfg( _xattr.aflags),  _xattr.fg,
        attribute_getfg(m_xattr.aflags), m_xattr.fg,
        sgrcap.sgrfg);

      update_sgrcolor(
        attribute_getbg( _xattr.aflags),  _xattr.bg,
        attribute_getbg(m_xattr.aflags), m_xattr.bg,
        sgrcap.sgrbg);

      this->m_attr = newAttr;
      this->m_xattr = _xattr;
    }

    if (this->sa_isSgrOpen)
      std::fputc(ascii_m, file);
  }

public:
  // test implementation
  // ToDo: output encoding
  void output_content(window& w) {
    for (curpos_t y = 0; y < w.m_height; y++) {
      window_cell* line = &w.cell(0, y);
      curpos_t wskip = 0;
      for (curpos_t x = 0; x < w.m_width; x++) {
        if (line[x].character&is_wide_extension) continue;
        if (line[x].character == 0) {
          wskip++;
          continue;
        }

        if (wskip > 0) {
          if (this->m_attr == 0 && wskip <= 4) {
            while (wskip--) std::fputc(' ', file);
          } else {
            std::fputc(ascii_esc, file);
            std::fputc(ascii_lbracket, file);
            put_unsigned(wskip);
            std::fputc(ascii_C, file);
          }
          wskip = 0;
        }

        ttysetattr(w, line[x].attribute);
        std::fputc(line[x].character, file);
      }

      std::fputc('\n', file);
    }
  }

};


int main() {
  contra::window w;
  w.resize(10, 10);

  w.cur.attribute = 196 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 202 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 220 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 154 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 63 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);

  tty_target target(stdout);
  target.output_content(w);

  return 0;
}
