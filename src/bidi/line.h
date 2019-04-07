// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_LINE_H
#define CONTRA_ANSI_LINE_H
#include <mwg/except.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>
#include "../contradef.h"

// debugging
#include "utf8.h"

namespace contra {
namespace ansi_term {

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

  struct line_t {
    std::vector<cell_t> cells;

    // int marker_count;
    // int cluster_count;
    // int object_count;

    int special_count;

  private:
    // m_monospace の時、データ位置と cells のインデックスは一致する。
    //   全角文字は wide_extension 文字を後ろにつける事により調節する。
    //   Grapheme cluster やマーカーは含まれない。
    // それ以外の時、データ位置と cells のインデックスは一致しない。
    //   各セルの幅を加算して求める必要がある。
    //   ゼロ幅の Grapheme cluster やマーカー等を含む事が可能である。
    bool m_monospace = true;

    void convert_to_proportional() {
      this->m_monospace = false;
      cells.erase(
        std::remove_if(cells.begin(), cells.end(),
          [] (cell_t const& cell) { return cell.character.is_wide_extension(); }),
        cells.end());
    }

  private:
    void monospace_put_character_at(std::size_t pos, cell_t const& cell) {
      std::size_t const w = cell.width;
      if (cells.size() < pos + w) {
        cell_t fill;
        fill.character = 0;
        fill.attribute = 0;
        fill.width = 1;
        cells.resize((std::size_t) (pos + w), fill);
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
      for (std::size_t q = pos + w; q < cells.size() && cells[q].character.is_extension(); q++) {
        cells[q].character = ascii_sp;
        cells[q].width = 1;
      }

      // 文字を書き込む
      cells[pos] = cell;
      for (std::size_t i = 1; i < w; i++) {
        cells[pos].character = character_t::flag_wide_extension;
        cells[pos].attribute = cell.attribute;
        cells[pos].width = 0;
      }
    }

  private:
    void proportional_put_character_at(std::size_t pos, cell_t const& cell, int implicit_move) {
      std::size_t const left = pos, right = pos + cell.width;

      // 置き換える範囲 i1..i2 を決定する。
      // Note: ゼロ幅の文字は "mark 文字 cluster" の様に
      //   文字の周りに分布している。通常は mark の直後に挿入する。
      //   つまり、ゼロ幅文字の直後に境界があると考える。
      std::size_t i1 = 0, x1 = 0;
      while (i1 < cells.size()) {
        std::size_t xnew = x1 + cells[i1].width;
        if (left < xnew) break;
        i1++;
        x1 = xnew;
      }
      std::size_t i2 = i1, x2 = x1;
      while (i2 < cells.size()) {
        std::size_t xnew = x2 + cells[i2].width;
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

      cell_t fill;
      fill.character = i1 < cells.size() ? ascii_sp : ascii_nul;
      fill.attribute = i1 < i2 ? cells[i1].attribute : 0;
      fill.width = 1;

      // 書き込み文字数の計算
      std::size_t count = 1;
      if (x1 < left) count += left - x1;
      if (right < x2) count += x2 - right;

      // 書き込み領域の確保
      if (count < i2 - i1) {
        cells.erase(cells.begin() + (i1 + count), cells.begin() + i2);
      } else if (count > i2 - i1) {
        cells.insert(cells.begin() + i2, i1 + count - i2, fill);
      }

      // 余白と文字の書き込み
      std::size_t i = i1;
      for (; x1 < left; x1++) cells[i++] = fill;
      cells[i++] = cell;
      if (right < x2) {
        fill.attribute = i1 < i2 ? cells[i2 - 1].attribute : 0;
        do cells[i++] = fill; while (right < --x2);
      }
    }

  public:
    void put_character_at(std::size_t pos, cell_t const& cell, int implicit_move) {
      if (m_monospace)
        monospace_put_character_at(pos, cell);
      else
        proportional_put_character_at(pos, cell, implicit_move);
    }

  public:
    void debug_init(const char* str) {
      cells.clear();
      int x = 0;
      while (*str) {
        cell_t cell;
        cell.character = (std::uint32_t) *str++;
        cell.attribute = 0;
        cell.width = 1;
        put_character_at(x, cell, cell.width);
        x += 2;
      }
    }

    void debug_print(std::FILE* file) {
      for (auto const& cell : cells) {
        char32_t c = (char32_t) cell.character.value;
        if (c == 0) c = 32;
        contra::encoding::put_u8(c, file);
      }
    }
  };
}
}

#endif
