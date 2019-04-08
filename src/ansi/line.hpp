// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_LINE_HPP
#define CONTRA_ANSI_LINE_HPP
#include <mwg/except.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include "../contradef.h"
#include "util.hpp"

// debugging
#include <cstdio>
#include "utf8.hpp"

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
    std::vector<cell_t> cells;
    line_attr_t m_lflags = {0};
    curpos_t m_home  {-1};
    curpos_t m_limit {-1};

    // m_monospace の時、データ位置と cells のインデックスは一致する。
    //   全角文字は wide_extension 文字を後ろにつける事により調節する。
    //   Grapheme cluster やマーカーは含まれない。
    // それ以外の時、データ位置と cells のインデックスは一致しない。
    //   各セルの幅を加算して求める必要がある。
    //   ゼロ幅の Grapheme cluster やマーカー等を含む事が可能である。
    bool m_monospace = true;

    void clear() {
      this->cells.clear();
      m_lflags = (line_attr_t) 0;
      m_home = -1;
      m_limit = -1;
      m_monospace = true;
    }

  private:
    void convert_to_proportional() {
      if (!this->m_monospace) return;
      this->m_monospace = false;
      cells.erase(
        std::remove_if(cells.begin(), cells.end(),
          [] (cell_t const& cell) { return cell.character.is_wide_extension(); }),
        cells.end());
    }

  private:
    void monospace_put_character_at(curpos_t pos, cell_t const& cell) {
      curpos_t const w = cell.width;
      curpos_t const ncell = (curpos_t) cells.size();
      if (ncell < pos + w) {
        cell_t fill;
        fill.character = 0;
        fill.attribute = 0;
        fill.width = 1;
        cells.resize((curpos_t) (pos + w), fill);
      }

      // 左側の中途半端な文字を消去
      if (cells[pos].character.is_extension()) {
        for (std::ptrdiff_t q = pos - 1; q >= 0; q--) {
          bool is_wide = cells[q].character.is_extension();
          cells[q].character = ascii_sp;
          cells[q].width = 1;
          if (!is_wide) break;
        }
      }

      // 右側の中途半端な文字を消去
      for (curpos_t q = pos + w; q < ncell && cells[q].character.is_extension(); q++) {
        cells[q].character = ascii_sp;
        cells[q].width = 1;
      }

      // 文字を書き込む
      cells[pos] = cell;
      for (curpos_t i = 1; i < w; i++) {
        cells[pos].character = character_t::flag_wide_extension;
        cells[pos].attribute = cell.attribute;
        cells[pos].width = 0;
      }
    }

  private:
    void proportional_put_character_at(curpos_t pos, cell_t const& cell, int implicit_move) {
      curpos_t const left = pos, right = pos + cell.width;
      curpos_t const ncell = (curpos_t) cells.size();

      // 置き換える範囲 i1..i2 を決定する。
      // Note: ゼロ幅の文字は "mark 文字 cluster" の様に
      //   文字の周りに分布している。通常は mark の直後に挿入する。
      //   つまり、ゼロ幅文字の直後に境界があると考える。
      curpos_t i1 = 0, x1 = 0;
      while (i1 < ncell) {
        curpos_t xnew = x1 + cells[i1].width;
        if (left < xnew) break;
        i1++;
        x1 = xnew;
      }
      curpos_t i2 = i1, x2 = x1;
      while (i2 < ncell) {
        curpos_t xnew = x2 + cells[i2].width;
        if (right < xnew) break;
        i2++;
        x2 = xnew;
      }

      if (implicit_move == 0) {
        // 暗黙移動を伴わない場合には直後の mark を残す様にする。
        while (i2 > 0 && cells[i2 - 1].is_zero_width_body()) i2--;
      } else if (implicit_move < 0) {
        // 逆方向に動いている場合は直後の mark を残し、
        // 直前の mark を削除する。
        while (i1 > 0 && cells[i1 - 1].is_zero_width_body()) i1--;
        while (i2 > 0 && cells[i2 - 1].is_zero_width_body()) i2--;
      }

      // 書き込み文字数の計算
      curpos_t count = 1;
      if (x1 < left) count += left - x1;
      if (right < x2) count += x2 - right;
      attribute_t lattr = 0, rattr = 0;
      if (i1 < i2) {
        lattr = cells[i1].attribute;
        rattr = cells[i2 - 1].attribute;
      }

      cell_t fill;
      fill.character = i1 < ncell ? ascii_sp : ascii_nul;
      fill.attribute = 0;
      fill.width = 1;

      // 書き込み領域の確保
      if (count < i2 - i1) {
        cells.erase(cells.begin() + (i1 + count), cells.begin() + i2);
      } else if (count > i2 - i1) {
        cells.insert(cells.begin() + i2, i1 + count - i2, fill);
      }

      // 余白と文字の書き込み
      curpos_t i = i1;
      if (x1 < left) {
        fill.attribute = lattr;
        do cells[i++] = fill; while (++x1 < left);
      }
      cells[i++] = cell;
      if (right < x2) {
        fill.attribute = rattr;
        do cells[i++] = fill; while (right < --x2);
      }
    }

  public:
    void put_character_at(curpos_t pos, cell_t const& cell, int implicit_move) {
      if (m_monospace)
        monospace_put_character_at(pos, cell);
      else
        proportional_put_character_at(pos, cell, implicit_move);
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
          for (auto const& cell : line.cells) {
            char32_t c = (char32_t) cell.character.value;
            if (c == 0) c = 32;
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
