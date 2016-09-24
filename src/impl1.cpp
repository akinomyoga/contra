#include "board.h"
#include "tty_observer.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>

using namespace contra;

void put_char(board& w, char32_t u) {
  board_cursor& cur = w.cur;
  int const charwidth = 1; // ToDo 文字幅
  if (cur.x + charwidth > w.m_width) {
    cur.x = 0;
    cur.y++;
    if (cur.y >= w.m_height) cur.y--; // 本当は新しい行を挿入する
  }

  if (charwidth <= 0) {
    std::exit(1); // ToDo
  }

  board_cell* c = &w.cell(cur.x, cur.y);
  w.set_character(c, u);
  w.set_attribute(c, cur.attribute);

  for (int i = 1; i < charwidth; i++) {
    w.set_character(c + i, is_wide_extension);
    w.set_attribute(c + i, 0);
  }

  cur.x += charwidth;
}


int main() {
  contra::board w;
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

  termcap_sgr_type sgrcap;
  sgrcap.initialize();

  tty_observer target(stdout, &sgrcap);
  target.print_screen(w);

  return 0;
}
