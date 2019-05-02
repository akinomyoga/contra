// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_ttty_buffer_hpp
#define contra_ttty_buffer_hpp
#include <cstdio>
#include <vector>
#include "../ansi/line.hpp"
#include "../ansi/term.hpp"
#include "../dict.hpp"

namespace contra {
namespace ttty {
  using namespace ::contra::ansi;
  using namespace ::contra::dict;

  struct tty_observer {
  private:
    term_t* term;

    // 端末の状態追跡の為の変数
    struct line_buffer_t {
      std::uint32_t id = (std::uint32_t) -1;
      std::uint32_t version = 0;
      std::vector<cell_t> content;
      int delta;
    };
    std::vector<line_buffer_t> screen_buffer;
    bool prev_decscnm = false;

    tty_writer w;

  public:
    tty_observer(term_t& term, std::FILE* file, termcap_sgr_type* sgrcap):
      term(&term), w(file, sgrcap) {}

    tty_writer& writer() { return w; }
    tty_writer const& writer() const { return w; }

  private:
    void put_csiseq_pn1(unsigned param, char ch) const {
      w.put(ascii_esc);
      w.put(ascii_left_bracket);
      if (param != 1) w.put_unsigned(param);
      w.put(ch);
    }
  private:
    curpos_t x = 0, y = 0;
    void move_to_line(curpos_t newy) {
      curpos_t const delta = newy - y;
      if (delta > 0) {
        put_csiseq_pn1(delta, ascii_B);
      } else if (delta < 0) {
        put_csiseq_pn1(-delta, ascii_A);
      }
      y = newy;
    }
    void move_to_column(curpos_t newx) {
      curpos_t const delta = newx - x;
      if (delta > 0) {
        put_csiseq_pn1(delta, ascii_C);
      } else if (delta < 0) {
        put_csiseq_pn1(-delta, ascii_D);
      }
      x = newx;
    }
    void move_to(curpos_t newx, curpos_t newy) {
      move_to_line(newy);
      move_to_column(newx);
    }
    void put_dl(curpos_t delta) const {
      if (delta > 0) put_csiseq_pn1(delta, ascii_M);
    }
    void put_il(curpos_t delta) const {
      if (delta > 0) put_csiseq_pn1(delta, ascii_L);
    }
    void put_ech(curpos_t delta) const {
      if (delta > 0) put_csiseq_pn1(delta, ascii_X);
    }
    void put_ich(curpos_t count) const {
      if (count > 0) put_csiseq_pn1(count, ascii_at);
    }
    void put_dch(curpos_t count) const {
      if (count > 0) put_csiseq_pn1(count, ascii_P);
    }

    /*?lwiki @var bool is_terminal_bottom;
     * 外側端末の一番下にいる時に `true` を設定する。
     * 一番下にいる時には一番下の行での IL を省略できる。
     * 一番下にいない時には DL の数と IL の数を合わせないと、
     * 下にある内容を破壊してしまう事になる。
     */
    static constexpr bool is_terminal_bottom = false;
    static constexpr bool is_terminal_fullwidth = true;

    /*?lwiki
     * @fn void trace_line_scroll();
     *   行の移動を追跡し、それに応じて IL, DL を用いたスクロールを実施します。
     *   同時に screen_buffer の内容も実施したスクロールに合わせて更新します。
     */
    void trace_line_scroll() {
      board_t const& b = term->board();

      curpos_t const height = b.m_height;
      curpos_t j = 0;
      for (curpos_t i = 0; i < height; i++) {
        screen_buffer[i].delta = height;
        for (curpos_t k = j; k < height; k++) {
          if (b.line(k).id() == screen_buffer[i].id) {
            screen_buffer[i].delta = k - i;
            j = k;
            break;
          }
        }
      }

      // DL による行削除
#ifndef NDEBUG
      int dbgD = 0, dbgI = 0, dbgC = 0;
#endif
      int previous_shift = 0;
      int total_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) continue;
        int const new_shift = screen_buffer[i].delta - previous_shift;
        if (new_shift < 0) {
          move_to_line(i + new_shift + total_shift);
          put_dl(-new_shift);
          total_shift += new_shift;
#ifndef NDEBUG
          dbgD -= new_shift;
#endif
        }
        previous_shift = screen_buffer[i].delta;
      }
      if (previous_shift > 0) {
        move_to_line(height - previous_shift + total_shift);
        put_dl(previous_shift);
#ifndef NDEBUG
        dbgD += previous_shift;
#endif
      }

      // IL による行追加
      previous_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) continue;
        int const new_shift = screen_buffer[i].delta - previous_shift;
        if (new_shift > 0) {
          move_to_line(i + previous_shift);
          put_il(new_shift);
#ifndef NDEBUG
          dbgI += new_shift;
#endif
        }
        previous_shift = screen_buffer[i].delta;
      }
      if (!is_terminal_bottom && previous_shift < 0) {
        move_to_line(height + previous_shift);
        put_il(-previous_shift);
#ifndef NDEBUG
        dbgI += -previous_shift;
#endif
      }

      // screen_buffer の更新
      previous_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) {
          screen_buffer[i].delta = previous_shift;
          continue;
        }
        int const new_shift = screen_buffer[i].delta - previous_shift;
        for (curpos_t j = i + new_shift; j < i; j++) {
          screen_buffer[j].delta = height;
          screen_buffer[j].content.clear();
#ifndef NDEBUG
          dbgC++;
#endif
        }
        previous_shift = screen_buffer[i].delta;
      }
      if (previous_shift > 0) {
        for (curpos_t j = height - previous_shift; j < height; j++) {
          screen_buffer[j].delta = height;
          screen_buffer[j].content.clear();
#ifndef NDEBUG
          dbgC++;
#endif
        }
      }
      for (curpos_t i = 0; i < height; i++) {
        curpos_t const i0 = i;
        for (curpos_t j = i0; j <= i; j++) {
          if (screen_buffer[j].delta == height) continue;
          if (j + screen_buffer[j].delta >= i)
            i = j + screen_buffer[j].delta;
        }
        for (curpos_t j = i; i0 <= j; j--) {
          int const delta = screen_buffer[j].delta;
          if (delta == height || delta == 0) continue;
          // j -> j + delta に移動して j にあった物は消去する
          std::swap(screen_buffer[j + delta], screen_buffer[j]);
          screen_buffer[j].id = -1;
          //screen_buffer[j].content.clear();
        }
      }
#ifndef NDEBUG
      mwg_assert(dbgD == dbgI && dbgI == dbgC,
        "total_delete=%d total_insert=%d clear=%d", dbgD, dbgI, dbgC);
#endif
    }
    void erase_until_eol() {
      board_t const& b = term->board();
      if (x >= b.m_width) return;

      attribute_t attr;
      w.apply_attr(attr);
      if (w.termcap_bce || attr.is_default()) {
        put_ech(b.m_width - x);
      } else {
        for (; x < b.m_width; x++) w.put(' ');
      }
    }

    void render_line(std::vector<cell_t> const& new_content, std::vector<cell_t> const& old_content) {
      // 更新の必要のある範囲を決定する

      // 一致する先頭部分の長さを求める。
      std::size_t i1 = 0;
      curpos_t x1 = 0;
      for (; i1 < new_content.size(); i1++) {
        cell_t const& cell = new_content[i1];
        if (i1 < old_content.size()) {
          if (cell != old_content[i1]) break;
        } else {
          if (!(cell.character == ascii_nul && cell.attribute.is_default())) break;
        }
        x1 += cell.width;
      }
      // Note: cluster_extension や marker に違いがある時は
      //   その前の有限幅の文字まで後退する。
      //   (出力先の端末がどの様に零幅文字を扱うのかに依存するが、)
      //   contra では古い marker を消す為には前の有限幅の文字を書く必要がある為。
      while (i1 && new_content[i1].width == 0) i1--;

      // 一致する末端部分のインデックスと長さを求める。
      auto _find_upper_bound_non_empty = [] (std::vector<cell_t> const& cells, std::size_t lower_bound) {
        std::size_t ret = cells.size();
        while (ret > lower_bound && cells[ret - 1].character == ascii_nul && cells[ret - 1].attribute.is_default()) ret--;
        return ret;
      };
      curpos_t w3 = 0;
      std::size_t i2 = _find_upper_bound_non_empty(new_content, i1);
      std::size_t j2 = _find_upper_bound_non_empty(old_content, i1);
      for (; j2 > i1 && i2 > i1; j2--, i2--) {
        cell_t const& cell = new_content[i2 - 1];
        if (new_content[i2 - 1] != old_content[j2 - 1]) break;
        w3 += cell.width;
      }
      // Note: cluster_extension や marker も本体の文字と共に再出力する。
      //   (出力先の端末がどの様に零幅文字を扱うのかに依存するが、)
      //   contra ではcluster_extension や marker は暗黙移動で潰される為。
      while (i2 < new_content.size() && new_content[i2].width == 0) i2++;

      // 間の部分の幅を求める。
      curpos_t new_w2 = 0, old_w2 = 0;
      for (std::size_t i = i1; i < i2; i++) new_w2 += new_content[i].width;
      for (std::size_t j = i1; j < j2; j++) old_w2 += old_content[j].width;

      move_to_column(x1);
      if (new_w2 || old_w2) {
        if (w3) {
          if (new_w2 > old_w2)
            put_ich(new_w2 - old_w2);
          else if (new_w2 < old_w2)
            put_dch(old_w2 - new_w2);
        }

        for (std::size_t i = i1; i < i2; i++) {
          cell_t const& cell = new_content[i];
          std::uint32_t code = cell.character.value;
          if (code == ascii_nul) code = ascii_sp;
          w.apply_attr(cell.attribute);
          w.put_u32(code);
          x += cell.width;
        }
      }

      move_to_column(x + w3);
      erase_until_eol();
    }

    void apply_default_attribute(std::vector<cell_t>& content) {
      tstate_t& s = term->state();
      if (!s.m_default_fg_space && !s.m_default_bg_space) return;
      for (auto& cell : content) {
        if (s.m_default_fg_space && cell.attribute.is_fg_default())
          cell.attribute.set_fg(s.m_default_fg_color, s.m_default_fg_space);
        if (s.m_default_bg_space && cell.attribute.is_bg_default())
          cell.attribute.set_bg(s.m_default_bg_color, s.m_default_bg_space);
      }
      board_t& b = term->board();
      if (content.size() < (std::size_t) b.m_width && (s.m_default_fg_space || s.m_default_bg_space)) {
        cell_t fill = ascii_nul;
        fill.attribute.set_fg(s.m_default_fg_color, s.m_default_fg_space);
        fill.attribute.set_bg(s.m_default_bg_color, s.m_default_bg_space);
        fill.width = 1;
        content.resize(b.m_width, fill);
      }
    }

  public:
    void update() {
      w.put(ascii_esc);
      w.put(ascii_left_bracket);
      w.put(ascii_question);
      w.put_unsigned(25);
      w.put(ascii_l);
      std::vector<cell_t> buff;

      bool full_update = false;
      if (prev_decscnm != term->state().get_mode(mode_decscnm)) {
        full_update = true;
        screen_buffer.clear();
        prev_decscnm = !prev_decscnm;
      }

      board_t const& b = term->board();
      screen_buffer.resize(b.m_height);
      if (is_terminal_fullwidth)
        trace_line_scroll();
      move_to(0, 0);
      for (curpos_t y = 0; y < b.m_height; y++) {
        line_t const& line = b.m_lines[y];
        line_buffer_t& line_buffer = screen_buffer[y];
        if (full_update || line_buffer.id != line.id() || line_buffer.version != line.version()) {
          line.get_cells_in_presentation(buff, b.line_r2l(line));
          this->apply_default_attribute(buff);
          this->render_line(buff, line_buffer.content);

          line_buffer.version = line.version();
          line_buffer.id = line.id();
          line_buffer.content.swap(buff);
          move_to_column(0);
        }

        if (y + 1 == b.m_height) break;
        w.put('\n');
        this->y++;
      }
      w.apply_attr(attribute_t {});

      move_to(b.cur.x(), b.cur.y());
      if (term->state().get_mode(mode_dectcem)) {
        w.put(ascii_esc);
        w.put(ascii_left_bracket);
        w.put(ascii_question);
        w.put_unsigned(25);
        w.put(ascii_h);
      }
      w.flush();
    }
  };

}
}
#endif
