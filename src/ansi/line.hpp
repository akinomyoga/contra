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
      flag_marker            = 0x30000000,
    };

    std::uint32_t value;

    character_t() {}
    character_t(std::uint32_t value): value(value) {}

    bool is_extension() const {
      return value & (flag_wide_extension | flag_cluster_extension);
    }
    bool is_wide_extension() const {
      return value & flag_wide_extension;
    }
  };

  struct attribute_t {
    std::uint32_t value;
    attribute_t() {}
    attribute_t(std::uint32_t value): value(value) {}
  };

  struct cell_t {
    character_t   character;
    attribute_t   attribute;
    std::uint32_t width;

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
    std::size_t m_prop_i;
    curpos_t m_prop_x;

  public:
    std::vector<cell_t> const& cells() const { return m_cells; }

    void clear() {
      this->m_cells.clear();
      m_lflags = (line_attr_t) 0;
      m_home = -1;
      m_limit = -1;
      m_prop_enabled = false;
      m_prop_i = 0;
      m_prop_x = 0;
    }

  private:
    void convert_to_proportional() {
      if (this->m_prop_enabled) return;
      this->m_prop_enabled = true;
      m_cells.erase(
        std::remove_if(m_cells.begin(), m_cells.end(),
          [] (cell_t const& cell) { return cell.character.is_wide_extension(); }),
        m_cells.end());
      m_prop_i = 0;
      m_prop_x = 0;
    }

  private:
    void monospace_write_cells(curpos_t pos, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      for (int i = 0; i < count; i++) {
        curpos_t const w = cell[i].width;
        if (w == 0) {
          convert_to_proportional();
          proportional_write_cells(pos, cell, count, repeat, implicit_move);
          return;
        }
        width += w;
      }
      width *= repeat;

      curpos_t const ncell = (curpos_t) this->m_cells.size();
      if (ncell < pos + width) {
        cell_t fill;
        fill.character = 0;
        fill.attribute = 0;
        fill.width = 1;
        this->m_cells.resize((curpos_t) (pos + width), fill);
      }

      // 左側の中途半端な文字を消去
      if (m_cells[pos].character.is_extension()) {
        for (std::ptrdiff_t q = pos - 1; q >= 0; q--) {
          bool is_wide = this->m_cells[q].character.is_extension();
          this->m_cells[q].character = ascii_sp;
          this->m_cells[q].width = 1;
          if (!is_wide) break;
        }
      }

      // 右側の中途半端な文字を消去
      for (curpos_t q = pos + width; q < ncell && this->m_cells[q].character.is_extension(); q++) {
        this->m_cells[q].character = ascii_sp;
        this->m_cells[q].width = 1;
      }

      // 文字を書き込む
      for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < count; i++) {
          curpos_t const w = cell[i].width;
          m_cells[pos] = cell[i];
          for (curpos_t i = 1; i < w; i++) {
            m_cells[pos + i].character = character_t::flag_wide_extension;
            m_cells[pos + i].attribute = cell[i].attribute;
            m_cells[pos + i].width = 0;
          }
          pos += w;
        }
      }
    }

  private:
    std::pair<std::size_t, curpos_t> proportional_glb(curpos_t xdst, bool include_zw_body) {
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
    std::pair<std::size_t, curpos_t> proportional_lub(curpos_t xdst, bool include_zw_body) {
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
    void proportional_write_cells(curpos_t pos, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      for (int i = 0; i < count; i++) width += cell[i].width;
      width *= repeat;
      curpos_t const left = pos, right = pos + width;
      std::size_t const ncell = m_cells.size();

      // 置き換える範囲 i1..i2 を決定する。
      // Note: ゼロ幅の文字は "mark 文字 cluster" の様に文字の周りに分布している。
      //   暗黙移動の方向の境界上の zw body (marker 等) は削除の対象とする。
      std::size_t i1, i2;
      curpos_t x1, x2;
      std::tie(i1, x1) = proportional_glb(left , implicit_move < 0);
      std::tie(i2, x2) = proportional_lub(right, implicit_move > 0);
      if (i1 > i2) i2 = i1; // cell.width=0 implicit_move=0 の時

      // 書き込み文字数の計算
      std::size_t nwrite = count * repeat;
      if (x1 < left) nwrite += left - x1;
      if (right < x2) nwrite += x2 - right;
      attribute_t lattr = 0, rattr = 0;
      if (i1 < i2) {
        lattr = m_cells[i1].attribute;
        rattr = m_cells[i2 - 1].attribute;
      }

      cell_t fill;
      fill.character = i1 < ncell ? ascii_sp : ascii_nul;
      fill.attribute = 0;
      fill.width = 1;

      // 書き込み領域の確保
      if (nwrite < i2 - i1) {
        m_cells.erase(m_cells.begin() + (i1 + nwrite), m_cells.begin() + i2);
      } else if (nwrite > i2 - i1) {
        m_cells.insert(m_cells.begin() + i2, i1 + nwrite - i2, fill);
      }

      // 余白と文字の書き込み
      curpos_t p = i1;
      if (x1 < left) {
        fill.attribute = lattr;
        do m_cells[p++] = fill; while (++x1 < left);
      }
      for (int r = 0; r < repeat; r++)
        for (int i = 0; i < count; i++)
          m_cells[p++] = cell[i];
      if (right < x2) {
        fill.attribute = rattr;
        do m_cells[p++] = fill; while (right < --x2);
      }
    }

  public:
    void write_cells(curpos_t pos, cell_t const* cell, int count, int repeat, int implicit_move) {
      if (m_prop_enabled)
        proportional_write_cells(pos, cell, count, repeat, implicit_move);
      else
        monospace_write_cells(pos, cell, count, repeat, implicit_move);
    }
  };

  struct cursor_t {
    curpos_t x, y;
    attribute_t attribute;
  };

  struct board_t {
    contra::util::ring_buffer<line_t> m_lines;

    cursor_t cur;
    int scroll_offset = 0;

    curpos_t m_width;
    curpos_t m_height;

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
      (void) y;
      return x; // ToDo:実装
    }
    curpos_t to_presentation_position(curpos_t y, curpos_t x) const {
      (void) y;
      return x; // ToDo:実装
    }

  public:
    void rotate(curpos_t count) {
      if (count > m_height) count = m_height;
      for (curpos_t i = 0; i < count; i++) {
        // ToDo: 消える行を何処かに記録する?
        m_lines[i].clear();
      }
      m_lines.rotate(count);
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
