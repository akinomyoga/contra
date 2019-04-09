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

      marker_base    = flag_marker | 0x00200000,
      marker_sds_l2r = marker_base + 1,
      marker_sds_r2l = marker_base + 2,
      marker_sds_end = marker_base + 3,
      marker_srs_beg = marker_base + 4,
      marker_srs_end = marker_base + 5,

      // Unicode bidi markers
      marker_lre = flag_marker | 0x202A,
      marker_rle = flag_marker | 0x202B,
      marker_pdf = flag_marker | 0x202C,
      marker_lro = flag_marker | 0x202D,
      marker_rlo = flag_marker | 0x202E,
      marker_lri = flag_marker | 0x2066,
      marker_rli = flag_marker | 0x2067,
      marker_fsi = flag_marker | 0x2068,
      marker_pdi = flag_marker | 0x2069,
    };

    std::uint32_t value;

    constexpr character_t(): value() {}
    constexpr character_t(std::uint32_t value): value(value) {}

    constexpr bool is_extension() const {
      return value & (flag_wide_extension | flag_cluster_extension);
    }
    constexpr bool is_wide_extension() const {
      return value & flag_wide_extension;
    }
    constexpr bool is_marker() const {
      return value & flag_marker;
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

    std::uint32_t m_version = 0;

    struct nested_string {
      curpos_t begin;
      curpos_t end;
      std::uint32_t end_marker;
      bool r2l;
      int parent;
    };
    mutable std::vector<nested_string> m_strings_cache;
    mutable bool m_strings_r2l = false;
    mutable std::uint32_t m_strings_version = (std::uint32_t) -1;

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
      m_version = 0;
      m_strings_cache.clear();
      m_strings_version = (std::uint32_t) -1;
      m_strings_r2l = false;
    }

  private:
    void convert_to_proportional() {
      if (this->m_prop_enabled) return;
      this->m_prop_enabled = true;
      this->m_version++;
      m_cells.erase(
        std::remove_if(m_cells.begin(), m_cells.end(),
          [] (cell_t const& cell) { return cell.character.is_wide_extension(); }),
        m_cells.end());
      m_prop_i = 0;
      m_prop_x = 0;
    }

  private:
    void monospace_write_cells(curpos_t pos, cell_t const* cell, int count, int repeat, int implicit_move) {
      this->m_version++;
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
        fill.character = ascii_nul;
        fill.attribute = 0;
        fill.width = 1;
        this->m_cells.resize((curpos_t) (pos + width), fill);
      }

      // 左側の中途半端な文字を消去
      if (m_cells[pos].character.is_extension()) {
        for (std::ptrdiff_t q = pos - 1; q >= 0; q--) {
          bool const is_wide = this->m_cells[q].character.is_extension();
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
      this->m_version++;
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

      // fill に使う文字を元の内容を元に決定しておく。
      cell_t lfill, rfill;
      if (x1 < left) {
        // Note: x1 < left && i1 < ncell の時点で x1 < left <= right <= x2 より i1 < i2 は保証される。
        // Note: 文字幅 2 以上の NUL が設置されている事があるか分からないが、その時には NUL で埋める。
        if (i1 < ncell) mwg_assert(i1 < i2);
        lfill.character = i1 < ncell && m_cells[i1].character.value != ascii_nul ? ascii_sp : ascii_nul;
        lfill.attribute = i1 < i2 ? m_cells[i1].attribute : 0;
        lfill.width = 1;
      }
      if (right < x2) {
        // Note: right < x2 の時点で x1 <= left <= right < x2 なので i1 < i2 が保証される。
        //   更に、i1 < i2 <= ncell も保証される。
        // Note: 文字幅 2 以上の NUL が設置されている事があるか分からないが、その時には NUL で埋める。
        mwg_assert(i1 < i2 && i1 < ncell);
        rfill.character = m_cells[i2 - 1].character.value != ascii_nul ? ascii_sp : ascii_nul;
        rfill.attribute = m_cells[i2 - 1].attribute;
        rfill.width = 1;
      }

      // 書き込み領域の確保
      if (nwrite < i2 - i1) {
        m_cells.erase(m_cells.begin() + (i1 + nwrite), m_cells.begin() + i2);
      } else if (nwrite > i2 - i1) {
        m_cells.insert(m_cells.begin() + i2, i1 + nwrite - i2, rfill);
      }

      // m_prop_i, m_prop_x 更新
      m_prop_i = i1 + nwrite;
      m_prop_x = std::max(x2, right);

      // 余白と文字の書き込み
      curpos_t p = i1;
      while (x1++ < left) m_cells[p++] = lfill;
      for (int r = 0; r < repeat; r++)
        for (int i = 0; i < count; i++)
          m_cells[p++] = cell[i];
      while (right < x2--) m_cells[p++] = rfill;
    }

  public:
    void write_cells(curpos_t pos, cell_t const* cell, int count, int repeat, int implicit_move) {
      if (m_prop_enabled)
        proportional_write_cells(pos, cell, count, repeat, implicit_move);
      else
        monospace_write_cells(pos, cell, count, repeat, implicit_move);
    }

  public:
    bool is_r2l(presentation_direction board_charpath) const {
      return is_charpath_rtol(board_charpath); // L: 0, R: 1
    }

    std::vector<nested_string> const& update_strings(curpos_t width, bool line_r2l) const {
      std::vector<nested_string>& ret = m_strings_cache;
      if (m_strings_version == m_version && m_strings_r2l == line_r2l) return ret;
      ret.clear();

      curpos_t x1 = 0;
      int istr = -1;
      auto _push = [&] (std::uint32_t end_marker, bool new_r2l) {
        nested_string str;
        str.begin = x1;
        str.end = -1;
        str.r2l = new_r2l;
        str.end_marker = end_marker;
        str.parent = istr;
        istr = ret.size();
        ret.emplace_back(str);
      };

      _push(0, line_r2l);
      ret[0].end = width;

      for (cell_t const& cell : m_cells) {
        std::uint32_t code = cell.character.value;
        if (cell.character.is_marker()) {
          switch (code) {
          case character_t::marker_sds_l2r: _push(character_t::marker_sds_end, false); break;
          case character_t::marker_sds_r2l: _push(character_t::marker_sds_end, true); break;
          case character_t::marker_srs_beg: _push(character_t::marker_srs_end, !ret[istr].r2l); break;
          case character_t::marker_sds_end:
          case character_t::marker_srs_end:
            while (istr >= 1) {
              bool const hit = ret[istr].end_marker == code;
              ret[istr].end = x1;
              istr = ret[istr].parent;
              if (hit) break;
            }
            break;

          default: break;
          }
        } else if (cell.character.value == ascii_nul) {
          while (istr >= 1) {
            ret[istr].end = x1;
            istr = ret[istr].parent;
          }
        } else {
          x1 += cell.width;
        }
      }
      while (istr >= 1) {
        ret[istr].end = x1;
        istr = ret[istr].parent;
      }

      m_strings_version = m_version;
      m_strings_r2l = line_r2l;
      return ret;
    }

  private:
    curpos_t convert_position(bool toPresentationPosition, curpos_t srcX, curpos_t width, bool line_r2l) const {
      if (!m_prop_enabled) return line_r2l ? std::max(0, width - srcX - 1) : srcX;
      std::vector<nested_string> const& strings = this->update_strings(width, line_r2l);
      if (strings.empty()) return line_r2l ? std::max(0, width - srcX - 1) : srcX;

      int r2l = 0;
      curpos_t x = toPresentationPosition ? 0 : srcX;
      for (nested_string const& range : strings) {
        curpos_t const referenceX = toPresentationPosition ? srcX : x;
        if (referenceX < range.begin) break;
        if (referenceX < range.end && range.r2l != r2l) {
          x = range.end - 1 - (x - range.begin);
          r2l = range.r2l;
        }
      }

      if (toPresentationPosition) {
        x = srcX - x;
        if (strings[0].r2l != r2l) x = -x;
      }

      return x;
    }

  public:
    curpos_t to_data_position(curpos_t x, curpos_t width, presentation_direction board_charpath) const {
      bool const line_r2l = is_r2l(board_charpath);
      // !m_prop_enabled の時は SDS/SRS も Unicode bidi も存在しない。
      if (!m_prop_enabled) return line_r2l ? std::max(0, width - x - 1) : x;
      return convert_position(false, x, width, line_r2l);
    }
    curpos_t to_presentation_position(curpos_t x, curpos_t width, presentation_direction board_charpath) const {
      bool const line_r2l = is_r2l(board_charpath);
      // !m_prop_enabled の時は SDS/SRS も Unicode bidi も存在しない。
      if (!m_prop_enabled) return line_r2l ? std::max(0, width - x - 1) : x;

      // キャッシュがある時はそれを使った方が速いだろう。
      if (m_strings_version == m_version && m_strings_r2l == line_r2l)
        return convert_position(true, x, width, line_r2l);

      struct nest_t {
        curpos_t beg;
        std::uint32_t end_marker;
        int  save_r2l;
        bool save_contains;
      };

      bool r2l = line_r2l; // false: L, true: R
      bool contains = false;

      std::vector<nest_t> stack;

      curpos_t a = 0, x1 = 0, x2 = 0;
      int contains_odd_count = -1;
      auto _pop = [&] () {
        if (contains) {
          if (r2l != line_r2l) {
            a += x1 - x2;
            contains_odd_count--;
          }
          x2 = x1;
        }
        r2l = stack.back().save_r2l;
        contains = stack.back().save_contains;
        stack.pop_back();
      };
      auto _push = [&] (std::uint32_t end_marker, bool new_r2l) {
        nest_t nest;
        nest.beg = x1;
        nest.end_marker = end_marker;
        nest.save_r2l = r2l;
        nest.save_contains = contains;
        stack.emplace_back(std::move(nest));
        r2l = new_r2l;
        contains = false;
      };

      for (cell_t const& cell : m_cells) {
        std::uint32_t code = cell.character.value;
        if (cell.character.is_marker()) {
          switch (code) {
          case character_t::marker_sds_l2r:
            _push(character_t::marker_sds_end, false);
            break;
          case character_t::marker_sds_r2l:
            _push(character_t::marker_sds_end, true);
            break;
          case character_t::marker_srs_beg:
            _push(character_t::marker_srs_end, !r2l);
            break;

          case character_t::marker_sds_end:
          case character_t::marker_srs_end:
            while (stack.size()) {
              bool const hit = stack.back().end_marker == code;
              _pop();
              if (hit) break;
            }
            break;

          default: break;
          }
        } else if (cell.character.value == ascii_nul) {
          // Note: HT に対しては HT (TAB) の代わりに NUL が挿入される。
          //   またその他の移動の後に文字を入れた時にも使われる。
          //   これらの NUL は segment separator として使われる。
          while (stack.size()) _pop();
        } else {
          if (x1 <= x && x < x1 + (curpos_t) cell.width) {
            contains_odd_count = 0;

            // 左側を集計しつつ contains マークを付ける。
            for (auto& nest : stack) {
              if (nest.save_r2l == line_r2l)
                a += nest.beg - x2;
              else
                contains_odd_count++;
              x2 = nest.beg;
              nest.save_contains = true;
            }

            if (r2l == line_r2l)
              a += x - x2;
            else
              contains_odd_count++;
            x2 = x + 1;
            contains = true;
          }
          x1 += cell.width;
        }

        // Note: 途中で見つかり更に反転範囲が全て閉じた時はその時点で確定。
        if (contains_odd_count == 0)
          return line_r2l ? width - a : a;
      }

      while (stack.size()) _pop();
      if (!contains) a = x;
      return line_r2l ? width - a : a;
    }

  public:
    void debug_dump() const {
      for (cell_t const& cell : m_cells) {
        std::uint32_t code = cell.character.value;
        if (cell.character.is_marker()) {
          switch (code) {
          case character_t::marker_sds_l2r: std::fprintf(stderr, "SDS(1)"); break;
          case character_t::marker_sds_r2l: std::fprintf(stderr, "SDS(2)"); break;
          case character_t::marker_srs_beg: std::fprintf(stderr, "SRS(1)"); break;
          case character_t::marker_sds_end: std::fprintf(stderr, "SDS(0)"); break;
          case character_t::marker_srs_end: std::fprintf(stderr, "SRS(1)"); break;
          default: break;
          }
        } else {
          std::putc((char) code, stderr);
        }
      }
      std::putc('\n', stderr);
    }
  };

  struct cursor_t {
    curpos_t x, y;
    attribute_t attribute;
  };

  struct board_t {
    contra::util::ring_buffer<line_t> m_lines;

    cursor_t cur;

    curpos_t m_width;
    curpos_t m_height;

    presentation_direction m_presentation_direction {presentation_direction_default};

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
      return m_lines[y].to_data_position(x, m_width, m_presentation_direction);
    }
    curpos_t to_presentation_position(curpos_t y, curpos_t x) const {
      return m_lines[y].to_presentation_position(x, m_width, m_presentation_direction);
    }

  public:
    void rotate(curpos_t count) {
      if (count > 0) {
        if (count > m_height) count = m_height;
        for (curpos_t i = 0; i < count; i++) {
          // ToDo: 消える行を何処かに記録する?
          m_lines[i].clear();
        }
        m_lines.rotate(count);
      } else if (count < 0) {
        count = -count;
        if (count < m_height) count = m_height;
        m_lines.rotate(m_lines.size() - count);
        for (curpos_t i = 0; i < count; i++)
          m_lines[i].clear();
      }
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
