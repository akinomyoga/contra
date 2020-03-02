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
    term_view_t* view;

    // 端末の状態追跡の為の変数
    frame_snapshot_t snapshot;
    std::vector<int> snapshot_delta;

    bool prev_decscnm = false;

    byte prev_fg_space = 0;
    byte prev_bg_space = 0;
    color_t prev_fg_color = 0;
    color_t prev_bg_color = 0;
    curpos_t prev_width = 0;
    curpos_t prev_height = 0;

    // 出力先端末の状態
    bool remote_dectcem = true;
    curpos_t remote_x = 0, remote_y = 0;
    curpos_t remote_w = 80, remote_h = 24;
    bool remote_xenl = true;

    tty_writer w;

  public:
    tty_observer(term_view_t& view, std::FILE* file, termcap_sgr_type* sgrcap):
      view(&view), w(view.atable(), file, sgrcap) {}

    tty_writer& writer() { return w; }
    tty_writer const& writer() const { return w; }

    void reset_size(curpos_t width, curpos_t height) {
      this->remote_w = width;
      this->remote_h = height;
    }
    void set_xenl(bool value) {
      this->remote_xenl = value;
    }

  private:
    void put_csiseq(char ch) const {
      w.put(ascii_esc);
      w.put(ascii_left_bracket);
      w.put(ch);
    }
    void put_csiseq_pn1(unsigned param, char ch) const {
      w.put(ascii_esc);
      w.put(ascii_left_bracket);
      if (param != 1) w.put_unsigned(param);
      w.put(ch);
    }

  private:
    void go_to(curpos_t newx, curpos_t newy) {
      w.put(ascii_esc);
      w.put(ascii_left_bracket);
      if (newy >= 1) w.put_unsigned(newy + 1);
      if (newx >= 1) {
        w.put(ascii_semicolon);
        w.put_unsigned(newx + 1);
      }
      w.put(ascii_H);
      remote_x = newx;
      remote_y = newy;
    }

  private:
    void move_to_line(curpos_t newy) {
      curpos_t const delta = newy - remote_y;
      if (delta > 0) {
        put_csiseq_pn1(delta, ascii_B);
      } else if (delta < 0) {
        put_csiseq_pn1(-delta, ascii_A);
      }
      remote_y = newy;
    }
    void move_to_column(curpos_t newx) {
      curpos_t const delta = newx - remote_x;
      if (delta > 0) {
        put_csiseq_pn1(delta, ascii_C);
      } else if (delta < 0) {
        put_csiseq_pn1(-delta, ascii_D);
      }
      remote_x = newx;
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
      curpos_t const height = view->height();
      snapshot_delta.resize(height, -1);
      curpos_t j = 0;
      for (curpos_t i = 0; i < height; i++) {
        snapshot_delta[i] = height;
        for (curpos_t k = j; k < height; k++) {
          if (view->line(k).id() == snapshot.lines[i].id) {
            snapshot_delta[i] = k - i;
            j = k + 1;
            break;
          }
        }
      }

#ifndef NDEBUG
      // for (curpos_t i = 0; i < height; i++) {
      //   if (snapshot.lines[i].id != (std::uint32_t) -1) {
      //     for (curpos_t k = i + 1; k < height; k++) {
      //       if (snapshot.lines[i].id == snapshot.dat[k].id) {
      //         std::fprintf(file, "ERROR DUPID: snapshot.lines[%d] and snapshot.lines[%d] has same id %d\n", i, k, snapshot.lines[i].id);
      //         break;
      //       }
      //     }
      //   }
      // }
#endif

      // DL による行削除
#ifndef NDEBUG
      int dbgD = 0, dbgI = 0, dbgC = 0;
#endif
      int previous_shift = 0;
      int total_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (snapshot_delta[i] == height) continue;
        int const new_shift = snapshot_delta[i] - previous_shift;
        if (new_shift < 0) {
          move_to_line(i + new_shift + total_shift);
          put_dl(-new_shift);
          total_shift += new_shift;
#ifndef NDEBUG
          dbgD -= new_shift;
#endif
        }
        previous_shift = snapshot_delta[i];
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
        if (snapshot_delta[i] == height) continue;
        int const new_shift = snapshot_delta[i] - previous_shift;
        if (new_shift > 0) {
          move_to_line(i + previous_shift);
          put_il(new_shift);
#ifndef NDEBUG
          dbgI += new_shift;
#endif
        }
        previous_shift = snapshot_delta[i];
      }
      if (!is_terminal_bottom && previous_shift < 0) {
        move_to_line(height + previous_shift);
        put_il(-previous_shift);
#ifndef NDEBUG
        dbgI += -previous_shift;
#endif
      }

      // snapshot の更新
      previous_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (snapshot_delta[i] == height) {
          snapshot_delta[i] = previous_shift;
          continue;
        }
        int const new_shift = snapshot_delta[i] - previous_shift;
        for (curpos_t j = i + new_shift; j < i; j++) {
          snapshot_delta[j] = height;
          snapshot.lines[j].content.clear();
#ifndef NDEBUG
          dbgC++;
#endif
        }
        previous_shift = snapshot_delta[i];
      }
      if (previous_shift > 0) {
        for (curpos_t j = height - previous_shift; j < height; j++) {
          snapshot_delta[j] = height;
          snapshot.lines[j].content.clear();
#ifndef NDEBUG
          dbgC++;
#endif
        }
      }
      for (curpos_t i = 0; i < height; i++) {
        curpos_t const i0 = i;
        for (curpos_t j = i0; j <= i; j++) {
          if (snapshot_delta[j] == height) continue;
          if (j + snapshot_delta[j] >= i)
            i = j + snapshot_delta[j];
        }
        for (curpos_t j = i; i0 <= j; j--) {
          int const delta = snapshot_delta[j];
          if (delta == height || delta == 0) continue;
          // j -> j + delta に移動して j にあった物は消去する
          std::swap(snapshot.lines[j + delta], snapshot.lines[j]);
          snapshot.lines[j].id = -1;
        }
      }
#ifndef NDEBUG
      mwg_assert(dbgD == dbgI && dbgI == dbgC,
        "total_delete=%d total_insert=%d clear=%d", dbgD, dbgI, dbgC);
#endif
    }

    struct apply_default_attribute_impl {
      term_view_t* view;
      byte fgspace;
      byte bgspace;
      color_t fgcolor;
      color_t bgcolor;

      attr_table* m_atable;

      bool hasprev = false;
      attr_t prev_attr = 0;
      attr_builder abuild;

      bool hasfill = false;
      cell_t fill;

      apply_default_attribute_impl(term_view_t* view):
        view(view), m_atable(view->atable()), abuild(view->atable())
      {
        fgspace = view->fg_space();
        bgspace = view->bg_space();
        fgcolor = view->fg_color();
        bgcolor = view->bg_color();
      }

      void initialize_fill() {
        if (hasfill) return;
        hasfill = true;

        attr_builder abuild(m_atable);
        abuild.set_fg(fgcolor, fgspace);
        abuild.set_bg(bgcolor, bgspace);
        fill.character = ascii_nul;
        fill.attribute = abuild.attr();
        fill.width = 1;
      }

      void apply(std::vector<cell_t>& content) {
        if (!fgspace && !bgspace) return;
        for (auto& cell : content) {
          if (hasprev && cell.attribute == prev_attr) {
            cell.attribute = abuild.attr();
            continue;
          }

          bool const setfg = fgspace && !m_atable->fg_space(cell.attribute);
          bool const setbg = bgspace && !m_atable->bg_space(cell.attribute);
          if (!setfg && !setbg) continue;

          hasprev = true;
          prev_attr = cell.attribute;
          abuild.set_attr(cell.attribute);
          if (setfg) abuild.set_fg(fgcolor, fgspace);
          if (setbg) abuild.set_bg(bgcolor, bgspace);
          cell.attribute = abuild.attr();
        }

        curpos_t const width = view->width();
        if (content.size() < (std::size_t) view->width() && (fgspace || bgspace)) {
          initialize_fill();
          content.resize(width, fill);
        }
      }
    };

    void erase_until_eol(attr_t const& fill_attr) {
      curpos_t const width = view->width();
      if (remote_x >= width) return;

      w.apply_attr(fill_attr);
      if (w.termcap_bce || view->atable()->is_default(fill_attr)) {
        put_ech(width - remote_x);
      } else {
        for (; remote_x < width; remote_x++) w.put(' ');
      }
    }

    void render_line(std::vector<cell_t> const& new_content, std::vector<cell_t> const& old_content, attr_t const& fill_attr) {
      // 更新の必要のある範囲を決定する
      attr_table* const atable = view->atable();

      // 一致する先頭部分の長さを求める。
      std::size_t i1 = 0;
      curpos_t x1 = 0;
      for (; i1 < new_content.size(); i1++) {
        cell_t const& cell = new_content[i1];
        if (i1 < old_content.size()) {
          if (cell != old_content[i1]) break;
        } else {
          if (!(cell.character == ascii_nul && atable->is_default(cell.attribute))) break;
        }
        x1 += cell.width;
      }
      // Note: cluster_extension や marker に違いがある時は
      //   その前の有限幅の文字まで後退する。
      //   (出力先の端末がどの様に零幅文字を扱うのかに依存するが、)
      //   contra では古い marker を消す為には前の有限幅の文字を書く必要がある為。
      while (i1 && new_content[i1].width == 0) i1--;

      // 一致する末端部分のインデックスと長さを求める。
      auto _find_upper_bound_non_empty = [atable] (std::vector<cell_t> const& cells, std::size_t lower_bound) {
        std::size_t ret = cells.size();
        while (ret > lower_bound && cells[ret - 1].character == ascii_nul && atable->is_default(cells[ret - 1].attribute)) ret--;
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
          if (remote_x + (curpos_t) cell.width > remote_w) return;
          w.apply_attr(cell.attribute);
          w.put_u32(code);
          remote_x += cell.width;
          if (!remote_xenl && remote_x == remote_w) {
            remote_y++;
            remote_x = 0;
            return;
          }
        }
      }

      move_to_column(remote_x + w3);
      erase_until_eol(fill_attr);
    }

    bool is_content_changed() const {
      curpos_t const height = view->height();
      if ((curpos_t) snapshot.lines.size() != height) return true;
      for (curpos_t y = 0; y < height; y++) {
        line_t const& line = view->line(y);
        auto const& snapshot_line = snapshot.lines[y];
        if (snapshot_line.id != line.id() || snapshot_line.version != line.version()) return true;
      }
      return false;
    }

    attr_t create_fill_attr() {
      attr_builder abuild(view->atable());
      abuild.set_bg(view->bg_color(), view->bg_space());
      return abuild.fill_attr();
    }

    void render_content(bool full_update) {
      std::vector<cell_t> buff;

      curpos_t const height = view->height();
      if (full_update) {
        snapshot.setup(*view);
        put_csiseq_pn1(2, ascii_J);
      }
      if (is_terminal_fullwidth)
        trace_line_scroll();

      attr_t const fill_attr = create_fill_attr();
      apply_default_attribute_impl default_attribute(view);
      for (curpos_t y = 0; y < height; y++) {
        line_t const& line = view->line(y);
        auto& snapshot_line = snapshot.lines[y];
        if (full_update || snapshot_line.id != line.id() || snapshot_line.version != line.version()) {
          go_to(0, y);
          view->order_cells_in(buff, position_client, line);
          default_attribute.apply(buff);
          this->render_line(buff, snapshot_line.content, fill_attr);

          snapshot_line.version = line.version();
          snapshot_line.id = line.id();
          snapshot_line.content.swap(buff);
        }
      }
      w.apply_attr(0);
    }

    void update_remote_dectcem(bool value) {
      if (remote_dectcem == value) return;
      remote_dectcem = value;
      w.put(ascii_esc);
      w.put(ascii_left_bracket);
      w.put(ascii_question);
      w.put_unsigned(25);
      w.put(value ? ascii_h : ascii_l);
    }

  public:
    void update() {
      view->update();

      bool full_update = false;
      auto _update_metric = [&] (auto& prev, auto value) {
        if (prev == value) return;
        prev = value;
        full_update = true;
      };
      _update_metric(prev_fg_space, view->fg_space());
      _update_metric(prev_fg_color, view->fg_color());
      _update_metric(prev_bg_space, view->bg_space());
      _update_metric(prev_bg_color, view->bg_color());
      _update_metric(prev_width, view->width());
      _update_metric(prev_height, view->height());

      if (full_update || is_content_changed()) {
        update_remote_dectcem(false);
        render_content(full_update);
      }

      if (0 <= view->y() && view->y() < view->height()) {
        go_to(view->x(), view->y());
        update_remote_dectcem(view->is_cursor_visible());
      } else {
        update_remote_dectcem(false);
      }
      w.flush();
    }
  };

}
}
#endif
