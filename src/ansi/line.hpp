// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_ansi_line_hpp
#define contra_ansi_line_hpp
#include <mwg/except.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <tuple>
#include <limits>
#include "../contradef.hpp"
#include "../util.hpp"
#include "attr.hpp"

// debugging
#include <cstdio>
#include "../enc.utf8.hpp"

namespace contra {
namespace ansi {

  struct character_t {
    std::uint32_t value;

    constexpr character_t(): value() {}
    constexpr character_t(std::uint32_t value): value(value) {}
    constexpr bool operator==(character_t const& rhs) const {
      return value == rhs.value;
    }
    constexpr bool operator!=(character_t const& rhs) const { return !(*this == rhs); }

  public:
    // global utility function
    static constexpr bool is_char(std::uint32_t value) {
      return !(value & ~unicode_mask);
    }

  public:
    constexpr bool is_extension() const {
      return value & (charflag_wide_extension | charflag_cluster_extension);
    }
    constexpr bool is_wide_extension() const {
      return value & charflag_wide_extension;
    }
    constexpr bool is_cluster_extension() const {
      return value & charflag_cluster_extension;
    }
    constexpr bool is_marker() const {
      return value & charflag_marker;
    }

  private:
    static bool get_unicode_from_iso2022(std::vector<char32_t>& buff, char32_t const value);

  public:
    bool get_unicode_representation(std::vector<char32_t>& buff) const {
      char32_t const ret = value & ~(charflag_marker | charflag_cluster_extension);
      if (ret <= unicode_max) {
        buff.clear();
        buff.push_back(ret);
        return true;
      }

      if (ret & charflag_iso2022)
        return get_unicode_from_iso2022(buff, ret);
      return false;
    }
  };

  struct cell_t {
    character_t   character;
    attr_t       attribute = 0;
    std::uint32_t width;

    cell_t() {}
    constexpr cell_t(std::uint32_t c):
      character(c),
      width(character.is_extension() || character.is_marker() ? 0 : 1) {}

    bool operator==(cell_t const& rhs) const {
      return character == rhs.character && attribute == rhs.attribute && width == rhs.width;
    }
    bool operator!=(cell_t const& rhs) const { return !(*this == rhs); }

    bool is_zero_width_body() const {
      return width == 0 && !character.is_extension();
    }
  };

  //---------------------------------------------------------------------------
  // line_attr_t

  typedef contra::util::flags_t<std::uint32_t, struct lattr_tag> line_attr_t;
  constexpr line_attr_t lattr_none = 0x0000;

  // bit 0
  constexpr line_attr_t lattr_used = 0x0001;

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
  constexpr line_attr_t lattr_rtol = 0x0002;
  constexpr line_attr_t lattr_ltor = 0x0004;
  constexpr line_attr_t lattr_charpath_mask = lattr_rtol | lattr_ltor;

  // bit 6-7: DECDHL, DECDWL, DECSWL
  // The same values are used as in `xflags_t`.
  // The related constants are defined in `enum extended_flags`.

  //---------------------------------------------------------------------------

  enum presentation_direction_t {
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

  constexpr bool is_charpath_rtol(presentation_direction_t presentationDirection) {
    return presentationDirection == presentation_direction_rltb
      || presentationDirection == presentation_direction_btlr
      || presentationDirection == presentation_direction_rlbt
      || presentationDirection == presentation_direction_btrl;
  }

  enum position_type {
    position_client       = 0,
    position_presentation = 1,
    position_data         = 2,
  };

  enum line_segment_flags {
    line_segment_slice,
    line_segment_erase,
    line_segment_space,
    line_segment_erase_unprotected,
    line_segment_transfer,
  };

  class line_t;

  struct line_segment_t {
    curpos_t p1;
    curpos_t p2;
    int type;

    /// @var source
    ///   type が line_segment_transfer の時に転送元を指定します。
    line_t* source = nullptr;
    bool source_r2l = false;
  };

  //---------------------------------------------------------------------------
  // line_shift_flags

  // for line_t::shift_cells()
  typedef contra::util::flags_t<std::uint32_t, struct lshift_tag> line_shift_flags;
  constexpr line_shift_flags lshift_none            = 0x00;
  constexpr line_shift_flags lshift_left_inclusive  = 0x01;
  constexpr line_shift_flags lshift_right_inclusive = 0x02;
  constexpr line_shift_flags lshift_dcsm            = 0x04;
  constexpr line_shift_flags lshift_r2l             = 0x08;

  /// @var lshift_erm_protect
  /// 消去の時 (abs(shift) >= p2 - p1 の時) に保護領域を消去しない事を表すフラグです。
  /// シフトの場合 (abs(shift) < p2 - p1 の時) には使われません。
  constexpr line_shift_flags lshift_erm_protect     = 0x10;

  //---------------------------------------------------------------------------

  enum word_selection_type {
    word_selection_cword,
    word_selection_sword,
  };

  class line_t {
  private:
    std::vector<cell_t> m_cells;
    attr_table* m_atable;
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
    mutable std::size_t m_prop_i;
    mutable curpos_t m_prop_x;

    std::uint32_t m_id = (std::uint32_t) -1;
    std::uint32_t m_version = 0;

    struct nested_string {
      curpos_t begin;
      curpos_t end;
      std::uint32_t beg_marker;
      std::uint32_t end_marker;
      bool r2l;
      int parent;
    };
    mutable std::vector<nested_string> m_strings_cache;
    mutable bool m_strings_r2l = false;
    mutable std::uint32_t m_strings_version = (std::uint32_t) -1;

  public:
    line_t(attr_table* atable): m_atable(atable) {}

  public:
    // 既定の move assignment は swap ではなくて解放&Moveになっている。
    // 後、キャッシュまで一緒に transfer する必要は全くないので省略。
    void transfer_from(line_t& line) {
      m_cells.swap(line.m_cells);
      m_lflags       = line.m_lflags;
      m_limit        = line.m_limit;
      m_prop_enabled = line.m_prop_enabled;
      m_prop_i       = line.m_prop_i;
      m_prop_x       = line.m_prop_x;
      m_id           = line.m_id;
      m_version      = line.m_version;

      // invalidate cache
      this->m_strings_version = -1;
      line.m_cells.clear();
      line.m_prop_enabled = false;
      line.m_strings_version = -1;
    }

  public:
    std::vector<cell_t>& cells() { return m_cells; }
    std::vector<cell_t> const& cells() const { return m_cells; }
    line_attr_t& lflags() { return m_lflags; }
    line_attr_t const& lflags() const { return m_lflags; }
    curpos_t& home() { return m_home; }
    curpos_t const& home() const { return m_home; }
    curpos_t& limit() { return m_limit; }
    curpos_t const& limit() const { return m_limit; }

  public:
    std::uint32_t version() const { return this->m_version; }
    std::uint32_t id() const { return m_id; }
    void set_id(std::uint32_t value) { this->m_id = value; }

    bool has_protected_cells() const {
      for (cell_t const& cell : m_cells)
        if (m_atable->is_protected(cell.attribute)) return true;
      return false;
    }
    bool has_blinking_cells() const {
      for (cell_t const& cell : m_cells)
        if (m_atable->is_blinking(cell.attribute)) return true;
      return false;
    }

  private:
    void _initialize_content(curpos_t width, attr_t const& attr) {
      if (attr == 0) return;
      cell_t fill;
      fill.character = ascii_nul;
      fill.attribute = attr;
      fill.width = 1;
      this->m_cells.resize(width, fill);
    }

  public:
    void clear() {
      this->clear_content();
      this->m_lflags = (line_attr_t) 0;
      this->m_home = -1;
      this->m_limit = -1;
    }
    void clear(curpos_t width, attr_t const& attr) {
      this->clear();
      this->_initialize_content(width, attr);
    }

    void clear_content() {
      this->m_cells.clear();
      this->m_prop_enabled = false;
      this->m_prop_i = 0;
      this->m_prop_x = 0;
      this->m_strings_cache.clear();
      this->m_strings_version = (std::uint32_t) -1;
      this->m_strings_r2l = false;
      this->m_version++;
    }
    void clear_content(curpos_t width, attr_t const& attr) {
      this->clear_content();
      this->_initialize_content(width, attr);
    }

    void gc_mark() {
      for (auto& cell : m_cells)
        m_atable->mark(&cell.attribute);
    }

  private:
    void convert_to_proportional() {
      if (this->m_prop_enabled) return;
      this->m_prop_enabled = true;
      this->m_version++;
      this->m_cells.erase(
        std::remove_if(m_cells.begin(), m_cells.end(),
          [] (cell_t const& cell) { return cell.character.is_wide_extension(); }),
        m_cells.end());
      this->m_prop_i = 0;
      this->m_prop_x = 0;
    }

  private:
    void _mono_generic_replace_cells(curpos_t xL, curpos_t xR, cell_t const* cell, int count, int repeat, curpos_t width, int implicit_move);
    void _mono_write_cells(curpos_t x, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      bool flag_zw = false;
      for (int i = 0; i < count; i++) {
        curpos_t const w = cell[i].width;
        if (w == 0) flag_zw = true;
        width += w;
      }
      width *= repeat;

      if (flag_zw) {
        convert_to_proportional();
        _prop_generic_replace_cells(x, x + width, cell, count, repeat, width, implicit_move);
      } else
        _mono_generic_replace_cells(x, x + width, cell, count, repeat, width, implicit_move);
    }
    void _mono_replace_cells(curpos_t x1, curpos_t x2, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      bool flag_zw = false;
      for (int i = 0; i < count; i++) {
        curpos_t const w = cell[i].width;
        if (w == 0) flag_zw = true;
        width += w;
      }
      width *= repeat;

      if (flag_zw) {
        convert_to_proportional();
        _prop_generic_replace_cells(x1, x2, cell, count, repeat, width, implicit_move);
      } else
        _mono_generic_replace_cells(x1, x2, cell, count, repeat, width, implicit_move);
    }
    void _mono_delete_cells(curpos_t x1, curpos_t x2) {
      _mono_generic_replace_cells(x1, x2, nullptr, 0, 0, 0, 0);
    }

  private:
    /*?lwiki
     * @fn auto [i, x] = _prob_glb(xdst, includeZwBody);
     * @param[in] xdst
     * @param[in] includeZwBody
     * 指定した位置 xdst に対応する文字境界のインデックス i と位置 x を取得します。
     * 特に xdst 以前の最後の文字境界を取得します。
     * `includeZwBody` が false/true の時、xdst にある零幅文字の後/前の境界を取得します。
     */
    std::pair<std::size_t, curpos_t> _prop_glb(curpos_t xdst, bool include_zw_body) const {
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
    /*?lwiki
     * @fn auto [i, x] = _prob_lub(xdst, includeZwBody);
     * @param[in] xdst
     * @param[in] includeZwBody
     * 指定した位置 xdst に対応する文字境界のインデックス i と位置 x を取得します。
     * 特に xdst 以後の最初の文字境界を取得します。
     * `includeZwBody` が false/true の時、xdst にある零幅文字の前/後の境界を取得します。
     */
    std::pair<std::size_t, curpos_t> _prop_lub(curpos_t xdst, bool include_zw_body) const {
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
    void _prop_generic_replace_cells(curpos_t xL, curpos_t xR, cell_t const* cell, int count, int repeat, curpos_t width, int implicit_move);
    void _prop_write_cells(curpos_t pos, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      for (int i = 0; i < count; i++) width += cell[i].width;
      width *= repeat;
      curpos_t const left = pos, right = pos + width;
      _prop_generic_replace_cells(left, right, cell, count, repeat, width, implicit_move);
    }
    void _prop_replace_cells(curpos_t x1, curpos_t x2, cell_t const* cell, int count, int repeat, int implicit_move) {
      curpos_t width = 0;
      for (int i = 0; i < count; i++) width += cell[i].width;
      width *= repeat;
      _prop_generic_replace_cells(x1, x2, cell, count, repeat, width, implicit_move);
    }
    void _prop_delete_cells(curpos_t x1, curpos_t x2) {
      _prop_generic_replace_cells(x1, x2, nullptr, 0, 0, 0, 0);
    }

  public:
    void write_cells(curpos_t x, cell_t const* cell, int count, int repeat, int implicit_move) {
      if (m_prop_enabled)
        _prop_write_cells(x, cell, count, repeat, implicit_move);
      else
        _mono_write_cells(x, cell, count, repeat, implicit_move);
    }
    void insert_cells(curpos_t x, cell_t const* cell, int count, int repeat) {
      if (m_prop_enabled)
        _prop_replace_cells(x, x, cell, count, repeat, 0);
      else
        _mono_replace_cells(x, x, cell, count, repeat, 0);
    }
    void delete_cells(curpos_t x1, curpos_t x2) {
      if (m_prop_enabled)
        _prop_delete_cells(x1, x2);
      else
        _mono_delete_cells(x1, x2);
    }
    void replace_cells(curpos_t x1, curpos_t x2, cell_t const* cell, int count, int repeat, int implicit_move) {
      if (m_prop_enabled)
        _prop_replace_cells(x1, x2, cell, count, repeat, implicit_move);
      else
        _mono_replace_cells(x1, x2, cell, count, repeat, implicit_move);
    }

  private:
    void _mono_reverse(curpos_t width) {
      if ((curpos_t) m_cells.size() != width)
        m_cells.resize(width, cell_t(ascii_nul));

      std::reverse(m_cells.begin(), m_cells.end());
      for (auto cell = m_cells.begin(), cellM = m_cells.end() - 1; cell < cellM; ++cell) {
        auto beg = cell;
        while (cell < cellM && cell->character.is_wide_extension()) ++cell;
        if (cell != beg) std::iter_swap(beg, cell);
      }

      m_version++;
    }
    void _prop_reverse(curpos_t width);
  public:
    void reverse(curpos_t width) {
      if (m_prop_enabled)
        _prop_reverse(width);
      else
        _mono_reverse(width);
    }

  public:
    /// @fn cell_t const& char_at(curpos_t x) const;
    /// 指定した位置にある有限幅の文字を取得します。
    /// @param[in] x データ位置を指定します。
    character_t char_at(curpos_t x) const {
      if (!m_prop_enabled) {
        if (x < 0 || (std::size_t) x >= m_cells.size()) return ascii_nul;
        //while (x > 0 && m_cells[x].character.is_extension()) x--;
        return m_cells[x].character;
      } else {
        std::size_t i1;
        curpos_t x1;
        std::tie(i1, x1) = _prop_glb(x, false);
        if (i1 >= m_cells.size()) return ascii_nul;
        if (x1 < x) return charflag_wide_extension;
        return m_cells[i1].character;
      }
    }

  public:
    bool is_r2l(presentation_direction_t board_charpath) const {
      if (m_lflags & lattr_ltor)
        return false;
      else if (m_lflags & lattr_rtol)
        return true;
      else
        return is_charpath_rtol(board_charpath);
    }

  public:
    std::vector<nested_string> const& update_strings(curpos_t width, bool line_r2l) const;

  private:
    curpos_t _prop_to_presentation_position(curpos_t x, bool line_r2l) const;
  public:
    /// @fn curpos_t convert_position(bool toPresentationPosition, curpos_t srcX, int edge_type, curpos_t width, bool line_r2l) const;
    ///   表示位置とデータ位置の間の変換を行います。
    /// @param[in] toPresentationPosition
    ///   true の時データ位置から表示位置に変換します。false の時データ位置から表示位置に変換します。
    /// @param[in] srcX
    ///   変換元の文字位置です (見た目のx座標ではない事に注意)。
    /// @param[in] edge_type
    ///   0 の時文字の位置として変換します。-1 の時、文字の左端として変換します。1 の時文字の右端として変換します。
    /// @param[in] width
    ///   画面の横幅を指定します。
    /// @param[in] line_r2l
    ///   その行の r2l 値を指定します。
    ///   その行の既定の文字進路 (表示部) と文字進行 (データ部) の向きが一致しない時に true を指定します。
    curpos_t convert_position(bool toPresentationPosition, curpos_t srcX, int edge_type, curpos_t width, bool line_r2l) const;
  public:
    curpos_t to_data_position(curpos_t x, curpos_t width, bool line_r2l) const {
      // !m_prop_enabled の時は SDS/SRS も Unicode bidi も存在しない。
      if (!m_prop_enabled) return x;
      return convert_position(false, x, 0, width, line_r2l);
    }
    curpos_t to_presentation_position(curpos_t x, curpos_t width, bool line_r2l) const {
      // !m_prop_enabled の時は SDS/SRS も Unicode bidi も存在しない。
      if (!m_prop_enabled) return x;
      // キャッシュがある時はそれを使った方が速いだろう。
      if (m_strings_version == m_version && m_strings_r2l == line_r2l)
        return convert_position(true, x, 0, width, line_r2l);
      return _prop_to_presentation_position(x, line_r2l);
    }
    curpos_t to_data_position(curpos_t x, curpos_t width, presentation_direction_t board_charpath) const {
      return to_data_position(x, width, is_r2l(board_charpath));
    }
    curpos_t to_presentation_position(curpos_t x, curpos_t width, presentation_direction_t board_charpath) const {
      return to_presentation_position(x, width, is_r2l(board_charpath));
    }

  public:
    curpos_t convert_position(position_type from, position_type to, curpos_t srcX, int edge_type, curpos_t width, bool line_r2l) const {
      if (from == to) return srcX;
      if (line_r2l && from == position_client)
        srcX = edge_type == 0 ? width - 1 - srcX : width - srcX;

      if (m_prop_enabled) {
        if (from == position_data) {
          if (edge_type == 0)
            srcX = to_presentation_position(srcX, width, line_r2l);
          else
            srcX = convert_position(false, srcX, edge_type, width, line_r2l);
        } else if (to == position_data)
          srcX = convert_position(true, srcX, edge_type, width, line_r2l);
      }

      if (line_r2l && to == position_client)
        srcX = edge_type == 0 ? width - 1 - srcX : width - srcX;
      return srcX;
    }
    curpos_t convert_position(position_type from, position_type to, curpos_t srcX, int edge_type, curpos_t width, presentation_direction_t board_charpath) const {
      return convert_position(from, to, srcX, edge_type, width, is_r2l(board_charpath));
    }

  private:
    void _prop_order_cells_in(std::vector<cell_t>& buff, position_type to, curpos_t width, bool line_r2l) const;
  public:
    void order_cells_in(std::vector<cell_t>& buff, position_type to, curpos_t width, bool line_r2l) const;

  public:
    typedef std::vector<std::pair<curpos_t, curpos_t>> slice_ranges_t;
    void calculate_data_ranges_from_presentation_range(slice_ranges_t& ret, curpos_t x1, curpos_t x2, curpos_t width, bool line_r2l) const;

  public:
    /// @fn std::size_t find_innermost_string(curpos_t x, bool left, curpos_t width, bool line_r2l) const;
    /// 指定した位置がどの入れ子文字列に属しているかを取得します。
    /// @param[in] x 入れ子状態を調べるデータ位置を指定します。
    /// @param[in] left 入れ子状態を調べる位置が境界上の零幅文字の左側か右側かを指定します。
    ///   この引数が true の時に境界上の零幅文字の左側に於ける状態を取得します。
    /// @return line_t::update_strings の戻り値配列に於けるインデックスを返します。
    std::size_t find_innermost_string(curpos_t x, bool left, curpos_t width, bool line_r2l) const {
      if (!m_prop_enabled) return 0;
      auto const& strings = this->update_strings(width, line_r2l);
      int parent = 0;
      mwg_check(strings.size() <= std::numeric_limits<int>::max());
      for (std::size_t i = 1; i < strings.size(); i++) {
        auto const& str = strings[i];
        if (str.end < x || (!left && str.end == x)) continue;
        while (str.parent < parent) {
          if (x < strings[parent].end || (left && x == strings[parent].end)) return parent;
          parent = strings[parent].parent;
        }
        if (x < str.begin || (left && x == str.begin)) return parent;
        parent = i;
      }
      while (parent) {
        if (x < strings[parent].end || (left && x == strings[parent].end)) return parent;
        parent = strings[parent].parent;
      }
      return parent;
    }

  private:
    void _mono_compose_segments(line_segment_t const* comp, int count, curpos_t width, attr_t const& fill_attr, bool line_r2l, bool dcsm);
    void _prop_compose_segments(line_segment_t const* comp, int count, curpos_t width, attr_t const& fill_attr, bool line_r2l, bool dcsm);
  public:
    /// @fn void compose_segments(line_segment_t const* comp, int count, curpos_t width, bool line_r2l, attr_t const& fill_attr);
    /// 表示部に於ける範囲を組み合わせて新しく行の内容を再構築します。
    void compose_segments(line_segment_t const* comp, int count, curpos_t width, attr_t const& fill_attr, bool line_r2l, bool dcsm = false) {
      if (!m_prop_enabled)
        _mono_compose_segments(comp, count, width, fill_attr, line_r2l, dcsm);
      else
        _prop_compose_segments(comp, count, width, fill_attr, line_r2l, dcsm);
    }
    void truncate(curpos_t width, bool line_r2l, bool dcsm) {
      line_segment_t const seg = {0, width, line_segment_slice};
      compose_segments(&seg, 1, width, (attr_t) 0, line_r2l, dcsm);
    }

  private:
    void _mono_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attr_t const& fill_attr);
    void _bdsm_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attr_t const& fill_attr);
    void _prop_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attr_t const& fill_attr);
  public:
    /*?lwiki
     * @fn void shift_cells(p1, p2, shift, flags, width, fill_atr);
     * @param[in] curpos_t p1, p2;
     *   移動が起こる範囲を指定します。この範囲の外側に影響はありません。
     * @param[in] curpos_t shift;
     *   移動量を指定します。負の値は左側に移動することを示します。
     * @param[in] line_shift_flags flags;
     *   細かい動作を制御するフラグを指定します。
     * @param[in] curpos_t width;
     *   行の幅を指定します。境界を超えて出て行った内容は失われます。
     * @param[in] attr_t const& fill_attr;
     *   移動後に残る余白を埋める描画属性を指定します。
     */
    void shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attr_t const& fill_attr) {
      if (!m_prop_enabled)
        _mono_shift_cells(p1, p2, shift, flags, width, fill_attr);
      else if (!(flags & lshift_dcsm))
        _bdsm_shift_cells(p1, p2, shift, flags, width, fill_attr);
      else
        _prop_shift_cells(p1, p2, shift, flags, width, fill_attr);
    }

  public:
    /*?lwiki
     * @fn bool set_selection(curpos_t x1, curpos_t x2, bool trunc, bool gatm, bool dcsm);
     * @fn bool set_selection_word(curpos_t x, word_selection_type type, bool gatm);
     * @fn bool clear_selection();
     * 選択範囲を設定・解除します。
     * @return 選択範囲の設定によって実際に行内容に変更があった時に true を返します。
     */
    bool set_selection(curpos_t x1, curpos_t x2, bool trunc, bool gatm, bool dcsm);
    bool set_selection_word(curpos_t x, word_selection_type type, bool gatm);
    bool clear_selection() { return set_selection(0, 0, false, true, true); }

    /*?lwiki
     * @fn curpos_t extract_selection(std::u32string& data);
     * @param[in] x
     *   ここに指定した x データ位置以降の選択済みセルを取得します。
     * @return 最初の選択セルのデータ部における x 位置を返します。
     */
    curpos_t extract_selection(std::u32string& data) const;

  public:
    void debug_string_nest(FILE* file, curpos_t width, presentation_direction_t board_charpath) const {
      bool const line_r2l = is_r2l(board_charpath);
      auto const& strings = this->update_strings(width, line_r2l);

      curpos_t x = 0;
      int parent = 0;
      std::fprintf(file, "[");
      for (std::size_t i = 1; i < strings.size(); i++) {
        auto const& range = strings[i];

        while (range.parent < parent) {
          int delta = strings[parent].end - x;
          if (delta) std::fprintf(file, "%d", delta);
          std::fprintf(file, "]"); // 抜ける
          x = strings[parent].end;
          parent = strings[parent].parent;
        }

        if (x < range.begin) {
          std::fprintf(file, "%d", range.begin - x);
          x = range.begin;
        }
        std::fprintf(file, "[");
        parent = i;
      }
      while (parent >= 0) {
        int delta = strings[parent].end - x;
        if (delta) std::fprintf(file, "%d", delta);
        std::fprintf(file, "]"); // 抜ける
        x = strings[parent].end;
        parent = strings[parent].parent;
      }
      std::putc('\n', file);
    }

  public:
    void debug_dump(FILE* file) const {
      for (cell_t const& cell : m_cells) {
        std::uint32_t code = cell.character.value;
        if (cell.character.is_marker()) {
          switch (code) {
          case marker_sds_l2r: std::fprintf(file, "\x1b[91mSDS(1)\x1b[m"); break;
          case marker_sds_r2l: std::fprintf(file, "\x1b[91mSDS(2)\x1b[m"); break;
          case marker_srs_beg: std::fprintf(file, "\x1b[91mSRS(1)\x1b[m"); break;
          case marker_sds_end: std::fprintf(file, "\x1b[91mSDS(0)\x1b[m"); break;
          case marker_srs_end: std::fprintf(file, "\x1b[91mSRS(1)\x1b[m"); break;
          default: break;
          }
        } else if (code == ascii_nul) {
          std::fprintf(file, "\x1b[37m@\x1b[m");
        } else {
          contra::encoding::put_u8(code, file);
        }
      }
      std::putc('\n', file);
    }
  };

  struct cursor_t {
  private:
    curpos_t m_x = 0, m_y = 0;
    bool m_xenl = false;

  public:
    attr_builder abuild;

  public:
    cursor_t(attr_table* atable): abuild(atable) {}

  public:
    curpos_t x() const { return m_x; }
    curpos_t y() const { return m_y; }

    bool xenl() const { return m_xenl; }
    void adjust_xenl() {
      if (m_xenl) {
        m_x--;
        m_xenl = false;
      }
    }

    void set_x(curpos_t x, bool xenl = false) {
      this->m_x = x;
      this->m_xenl = xenl;
    }
    void set_y(curpos_t y) {
      this->m_y = y;
    }
    void set(curpos_t x, curpos_t y, bool xenl = false) {
      this->m_x = x;
      this->m_y = y;
      this->m_xenl = xenl;
    }
    void set_xenl(bool value) {
      this->m_xenl = value;
    }
    void update_x(curpos_t x) {
      if (m_x != x) set_x(x);
    }

    bool is_sane(curpos_t width) const {
      return (0 <= m_x && m_x < width) || (m_x == width && m_xenl);
    }
  };

}
}

#endif
