#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace tty1 {

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

    is_fg_color_set  = 0x00010000,
    is_bg_color_set  = 0x00020000,
    is_bold_set      = 0x00040000,
    is_underline_set = 0x00080000,
    is_stroke_set    = 0x00100000,
    is_invisible_set = 0x00200000,
    is_italic_set    = 0x00400000,

    has_extended_attribute = 0x8000,
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

using namespace tty1;

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
          if (wskip <= 4) {
            while (wskip--) std::putc(' ', file);
          } else
            std::fprintf(file, "\033[%dC", wskip);
          wskip = 0;
        }

        std::putc(line[x].character, file);
      }

      std::putc('\n', file);
    }
  }

};


int main() {
  tty1::window w;
  w.resize(10, 10);

  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);

  terminal_target target(stdout);
  target.output_content(w);

  return 0;
}
