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
    is_reverse_set          = 0x01000000, // ANSI \e[7m
    is_invisible_set        = 0x02000000, // ANSI \e[8m
    is_strike_set           = 0x04000000, // ANSI \e[9m
    is_fraktur_set          = 0x08000000, // ANSI \e[20m
    is_double_underline_set = 0x10000000, // ANSI \e[21m

    has_extended_attribute = 0x8000, // not supported yet
  };

  struct window_cell {
    std::uint32_t character {0};
    std::uint32_t attribute {0};
  };

  struct window_cursor {
    int x {0};
    int y {0};
    std::uint32_t attribute {0};
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

struct terminal_target {
  std::FILE* file;

  terminal_target(std::FILE* file): file(file) {}

  void put_str(const char* s) {
    while (*s) std::fputc(*s++, file);
  }
  void put_unsigned(unsigned value) {
    if (value >= 10)
      put_unsigned(value/10);
    std::fputc('0' + value % 10, file);
  }

  void sa_open_sgr() {
    if (this->sa_isSgrOpen) {
      std::fputc(';', file);
    } else {
      this->sa_isSgrOpen = true;
      std::fputc('\033', file);
      std::fputc('[', file);
    }
  }


  std::uint32_t attr {0};
  bool sa_isSgrOpen;

  struct sgrcap_color_type {
    int base;
    int bright_base;
    bool iso8613_indexed_color;
    char iso8613_separater;
  };

  struct sgrcap_type {
    sgrcap_color_type sgrfg {30,  90, true, ';'};
    sgrcap_color_type sgrbg {40, 100, true, ';'};
  } sgrcap;

  void update_sgrflag1(
    std::uint32_t newAttr,
    std::uint32_t bit, unsigned sgr, unsigned sgrReset
  ) {
    if (((newAttr ^ this->attr) & bit) == 0) return;

    sa_open_sgr();
    if (newAttr&is_reverse_set) {
      put_unsigned(sgr);
    } else {
      put_unsigned(sgrReset);
    }
  }

  void update_sgrflag2(
    std::uint32_t newAttr,
    std::uint32_t bit1, unsigned sgr1,
    std::uint32_t bit2, unsigned sgr2,
    unsigned sgrReset
  ) {
    if (((newAttr ^ this->attr) & (bit1 | bit2)) == 0) return;

    std::uint32_t const added = newAttr & ~this->attr;
    std::uint32_t const removed = ~newAttr & this->attr;

    sa_open_sgr();
    if (removed & (bit1 | bit2)) {
      put_unsigned(sgrReset);
      if (newAttr&bit2) {
        std::fputc(';', file);
        put_unsigned(sgr2);
      }
      if (newAttr&bit1) {
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

  void update_color(std::uint32_t newAttr, sgrcap_color_type const& cap, int isset, int mask, int shift) {
    if (((this->attr ^ newAttr) & (isset | mask)) == 0) return;

    std::uint32_t const added = newAttr & ~this->attr;
    std::uint32_t const removed = ~newAttr & this->attr;
    if (removed & isset) {
      sa_open_sgr();
      put_unsigned(cap.base + 9);
    } else if (added & isset || (this->attr ^ newAttr) & mask ) {
      sa_open_sgr();
      int const fg = unsigned(newAttr & mask) >> shift;
      if (fg < 8) {
        // e.g \e[31m
        put_unsigned(cap.base + fg);
      } else if (fg < 16 && cap.bright_base) {
        // e.g. \e[91m
        put_unsigned(cap.bright_base + (fg & 7));
      } else if (cap.iso8613_indexed_color) {
        // e.g. \e[38;5;17m
        put_unsigned(cap.base + 8);
        std::fputc(cap.iso8613_separater, file);
        put_unsigned(5);
        std::fputc(cap.iso8613_separater, file);
        put_unsigned(fg);
      } else {
        // todo: fallback
        // todo: \e[1;32m で明るい色を表示する端末
        put_unsigned(cap.base + 9);
      }
    }
  }
  void set_attribute(std::uint32_t newAttr) {
    if (this->attr == newAttr) return;

    this->sa_isSgrOpen = false;

    if (newAttr == 0) {
      sa_open_sgr();
    } else {
      update_sgrflag2(newAttr, is_bold_set     , 1, is_faint_set           ,  2, 22);
      update_sgrflag2(newAttr, is_italic_set   , 3, is_fraktur_set         , 20, 23);
      update_sgrflag2(newAttr, is_underline_set, 4, is_double_underline_set, 21, 24);
      update_sgrflag2(newAttr, is_blink_set    , 5, is_rapid_blink_set     ,  6, 25);

      update_sgrflag1(newAttr, is_reverse_set  , 7, 27);
      update_sgrflag1(newAttr, is_invisible_set, 8, 28);
      update_sgrflag1(newAttr, is_strike_set   , 9, 29);

      update_color(newAttr, sgrcap.sgrfg, is_fg_color_set, fg_color_mask, fg_color_shift);
      update_color(newAttr, sgrcap.sgrbg, is_bg_color_set, bg_color_mask, bg_color_shift);
    }

    if (this->sa_isSgrOpen)
      std::fputc('m', file);

    this->attr = newAttr;
  }

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

  terminal_target target(stdout);
  target.output_content(w);

  return 0;
}
