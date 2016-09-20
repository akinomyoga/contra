#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace contra {

  enum cell_character {
    unicode_mask       = 0x001FFFFF,
    is_acs_character   = 0x01000000,
    is_wide_extension  = 0x02000000,
    is_unicode_cluster = 0x04000000,
    is_embedded_object = 0x08000000,
  };

  enum cell_attribute {
    fg_color_mask = 0x00FF,
    bg_color_mask = 0xFF00,
    fg_color_shift = 0,
    bg_color_shift = 8,

    is_fg_color_set         = 0x00010000,
    is_bg_color_set         = 0x00020000,
    is_bold_set             = 0x00040000, // ANSI \e[1m
    is_faint_set            = 0x00080000, // ANSI \e[2m
    is_italic_set           = 0x00100000, // ANSI \e[3m
    is_underline_set        = 0x00200000, // ANSI \e[4m
    is_blink_set            = 0x00400000, // ANSI \e[5m
    is_rapid_blink_set      = 0x00800000, // ANSI \e[6m
    is_inverse_set          = 0x01000000, // ANSI \e[7m
    is_invisible_set        = 0x02000000, // ANSI \e[8m
    is_strike_set           = 0x04000000, // ANSI \e[9m
    is_fraktur_set          = 0x08000000, // ANSI \e[20m
    is_double_underline_set = 0x10000000, // ANSI \e[21m

    has_extended_attribute = 0x8000, // not supported yet
  };

  typedef std::uint32_t attribute_type;

  struct window_cell {
    std::uint32_t character {0};
    attribute_type attribute {0};
  };

  struct window_cursor {
    int x {0};
    int y {0};
    attribute_type attribute {0};
  };

  struct window {
    int m_width  {0};
    int m_height {0};

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
  };

}

using namespace contra;

void put_char(window& w, std::uint32_t u) {
  window_cursor& cur = w.cur;
  int const charwidth = 1; // ToDo
  if (cur.x + charwidth > w.m_width) {
    cur.x = 0;
    cur.y++;
    if (cur.y >= w.m_height) cur.y--; // 本当は新しい行を挿入する
  }

  if (charwidth <= 0) {
    std::exit(1); // ToDo
  }

  // ToDo: 文字幅
  window_cell* c = &w.cell(cur.x, cur.y);
  c->character = u;
  c->attribute = cur.attribute;
  for (int i = 1; i < charwidth; i++) {
    c[i].character = is_wide_extension;
    c[i].attribute = cur.attribute;
  }

  cur.x += charwidth;
}

struct termcap_sgrcolor {
  attribute_type bit;
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
  attribute_type bit;
  unsigned       on;
  unsigned       off;
};

struct termcap_sgrflag2 {
  attribute_type bit1;
  unsigned       on1;
  attribute_type bit2;
  unsigned       on2;
  unsigned       off;
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

  termcap_sgrcolor sgrfg {is_fg_color_set, 30, 39,  90, 38, ';', 0, 0};
  termcap_sgrcolor sgrbg {is_bg_color_set, 40, 49, 100, 48, ';', 0, 0};

  attribute_type attrNotResettable {0};

private:
  void initialize_attrNotResettable(termcap_sgrflag1 const& sgrflag) {
    if (!sgrflag.off && sgrflag.on) attrNotResettable |= sgrflag.bit;
  }

  void initialize_attrNotResettable(termcap_sgrflag2 const& sgrflag) {
    if (!sgrflag.off) {
      if (sgrflag.on1) attrNotResettable |= sgrflag.bit1;
      if (sgrflag.on2) attrNotResettable |= sgrflag.bit2;
    }
  }

  void initialize_attrNotResettable(termcap_sgrcolor const& sgrcolor) {
    if (!sgrcolor.off && (sgrcolor.base || sgrcolor.iso8613_indexed_color))
      attrNotResettable = sgrcolor.bit;
  }

public:
  void initialize() {
    attrNotResettable = 0;
    initialize_attrNotResettable(bold);
    initialize_attrNotResettable(italic);
    initialize_attrNotResettable(underline);
    initialize_attrNotResettable(blink);
    initialize_attrNotResettable(inverse);
    initialize_attrNotResettable(invisible);
    initialize_attrNotResettable(strike);
    initialize_attrNotResettable(sgrfg);
    initialize_attrNotResettable(sgrbg);
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
    std::fputc('0' + value % 10, file);
  }

private:
  void sa_open_sgr() {
    if (this->sa_isSgrOpen) {
      std::fputc(';', file);
    } else {
      this->sa_isSgrOpen = true;
      std::fputc('\033', file);
      std::fputc('[', file);
    }
  }

  attribute_type attr {0};
  bool sa_isSgrOpen;

  void update_sgrflag1(
    attribute_type newAttr,
    termcap_sgrflag1 const& sgrflag
  ) {
    // when the attribute is not changed
    if (((newAttr ^ this->attr) & sgrflag.bit) == 0) return;

    // when the attribute is not supported by TERM
    if (!sgrflag.on) return;

    sa_open_sgr();
    if (newAttr&sgrflag.bit) {
      put_unsigned(sgrflag.on);
    } else {
      put_unsigned(sgrflag.off);
    }
  }

  void update_sgrflag2(
    attribute_type newAttr,
    termcap_sgrflag2 const& sgrflag
  ) {
    attribute_type bit1 = sgrflag.bit1;
    attribute_type bit2 = sgrflag.bit2;

    // when the attribute is not changed
    if (((newAttr ^ this->attr) & (bit1 | bit2)) == 0) return;

    // when the attribute is not supported by TERM
    unsigned const sgr1 = sgrflag.on1;
    unsigned const sgr2 = sgrflag.on2;
    if (!sgr1) bit1 = 0;
    if (!sgr2) bit2 = 0;
    if (!(bit1 | bit2)) return;

    attribute_type const added = newAttr & ~this->attr;
    attribute_type const removed = ~newAttr & this->attr;

    sa_open_sgr();
    if (removed & (bit1 | bit2)) {
      put_unsigned(sgrflag.off);
      if (newAttr & bit2) {
        std::fputc(';', file);
        put_unsigned(sgr2);
      }
      if (newAttr & bit1) {
        std::fputc(';', file);
        put_unsigned(sgr1);
      }
    } else {
      bool isOutput = false;
      if (added & bit2) {
        put_unsigned(sgr2);
        isOutput = true;
      }
      if (added & bit1) {
        if (isOutput) std::fputc(';', file);
        put_unsigned(sgr1);
      }
    }
  }

  void update_sgrcolor(attribute_type newAttr, termcap_sgrcolor const& sgrcolor, attribute_type mask, int shift) {
    attribute_type const isset = sgrcolor.bit;
    if (((this->attr ^ newAttr) & (isset | mask)) == 0) return;

    attribute_type const added = newAttr & ~this->attr;
    attribute_type const removed = ~newAttr & this->attr;
    if (removed & isset) {
      sa_open_sgr();
      put_unsigned(sgrcolor.off);
      return;
    }

    if (added & isset || (this->attr ^ newAttr) & mask) {
      sa_open_sgr();
      unsigned const fg = unsigned(newAttr & mask) >> shift;
      if (fg < 8) {
        if (sgrcolor.base) {
          // e.g \e[31m
          if (sgrcolor.high_intensity_off)
            put_unsigned(sgrcolor.high_intensity_off);
          put_unsigned(sgrcolor.base + fg);
          return;
        }
      } else if (fg < 16) {
        if (sgrcolor.aixterm_color) {
          // e.g. \e[91m
          put_unsigned(sgrcolor.aixterm_color + (fg & 7));
          return;
        } else if (sgrcolor.base && sgrcolor.high_intensity_on) {
          // e.g. \e[1;31m \e[2;42m
          put_unsigned(sgrcolor.high_intensity_on);
          put_unsigned(sgrcolor.base + (fg & 7));
          return;
        }
      }

      if (sgrcolor.iso8613_indexed_color) {
        // e.g. \e[38;5;17m
        put_unsigned(sgrcolor.iso8613_indexed_color);
        std::fputc(sgrcolor.iso8613_separater, file);
        put_unsigned(5);
        std::fputc(sgrcolor.iso8613_separater, file);
        put_unsigned(fg);
        return;
      }

      // todo: fallback
      put_unsigned(sgrcolor.off);
    }
  }

  void set_attribute(attribute_type newAttr) {
    if (this->attr == newAttr) return;

    this->sa_isSgrOpen = false;

    if (newAttr == 0) {
      sa_open_sgr();
      return;
    }

    attribute_type const removed = ~newAttr & this->attr;
    if (removed & sgrcap.attrNotResettable) {
      sa_open_sgr();
      this->attr = 0;
    }

    update_sgrflag2(newAttr, sgrcap.bold     );
    update_sgrflag2(newAttr, sgrcap.italic   );
    update_sgrflag2(newAttr, sgrcap.underline);
    update_sgrflag2(newAttr, sgrcap.blink    );

    update_sgrflag1(newAttr, sgrcap.inverse  );
    update_sgrflag1(newAttr, sgrcap.invisible);
    update_sgrflag1(newAttr, sgrcap.strike   );

    update_sgrcolor(newAttr, sgrcap.sgrfg, fg_color_mask, fg_color_shift);
    update_sgrcolor(newAttr, sgrcap.sgrbg, bg_color_mask, bg_color_shift);

    if (this->sa_isSgrOpen)
      std::fputc('m', file);

    this->attr = newAttr;
  }

public:
  // test implementation
  // ToDo: output encoding
  void output_content(window& w) {
    for (int y = 0; y < w.m_height; y++) {
      window_cell* line = &w.cell(0, y);
      int wskip = 0;
      for (int x = 0; x < w.m_width; x++) {
        if (line[x].character&is_wide_extension) continue;
        if (line[x].character == 0) {
          wskip++;
          continue;
        }

        if (wskip > 0) {
          if (this->attr == 0 && wskip <= 4) {
            while (wskip--) std::fputc(' ', file);
          } else
            std::fprintf(file, "\033[%dC", wskip);
          wskip = 0;
        }

        set_attribute(line[x].attribute);
        std::fputc(line[x].character, file);
      }

      std::fputc('\n', file);
    }
  }

};


int main() {
  contra::window w;
  w.resize(10, 10);

  w.cur.attribute = 196;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 202;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 220;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 154;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 63;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);

  tty_target target(stdout);
  target.output_content(w);

  return 0;
}
