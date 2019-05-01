// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_ansi_line_hpp
#define contra_ansi_line_hpp
#include <mwg/except.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <tuple>
#include "../contradef.hpp"
#include "../dict.hpp"
#include "../util.hpp"

// debugging
#include <cstdio>
#include "../enc.utf8.hpp"

namespace contra {
namespace ansi {
  using ::contra::dict::attribute_t;
  using ::contra::dict::color_t;
  using ::contra::dict::xflags_t;
  using ::contra::dict::aflags_t;

  struct character_t {
    enum flags {
      unicode_mask       = 0x001FFFFF,
      flag_acs_character   = 0x01000000, // not yet supported
      flag_embedded_object = 0x08000000, // not yet supported

      flag_wide_extension    = 0x10000000,
      flag_cluster_extension = 0x20000000,
      flag_marker            = 0x40000000,

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
    constexpr bool operator==(character_t const& rhs) const {
      return value == rhs.value;
    }
    constexpr bool operator!=(character_t const& rhs) const { return !(*this == rhs); }

    constexpr bool is_extension() const {
      return value & (flag_wide_extension | flag_cluster_extension);
    }
    constexpr bool is_wide_extension() const {
      return value & flag_wide_extension;
    }
    constexpr bool is_cluster_extension() const {
      return value & flag_cluster_extension;
    }
    constexpr bool is_marker() const {
      return value & flag_marker;
    }
  };

  struct cell_t {
    character_t   character;
    attribute_t   attribute;
    std::uint32_t width;

    cell_t() {}
    constexpr cell_t(std::uint32_t c):
      character(c), attribute(0),
      width(character.is_extension() || character.is_marker() ? 0 : 1) {}

    bool operator==(cell_t const& rhs) const {
      return character == rhs.character && attribute == rhs.attribute && width == rhs.width;
    }
    bool operator!=(cell_t const& rhs) const { return !(*this == rhs); }

    bool is_zero_width_body() const {
      return width == 0 && !character.is_extension();
    }
    bool is_protected() const {
      return attribute.xflags & (attribute_t::spa_protected | attribute_t::daq_protected);
    }
    bool is_decsca_protected() const {
      return attribute.is_decsca_protected();
    }
  };

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

  // for line_t::shift_cells()
  struct line_shift_flags: contra::util::flags<std::uint32_t, line_shift_flags> {
    using base = contra::util::flags<std::uint32_t, line_shift_flags>;
    using def = base::def;

    template<typename... Args>
    line_shift_flags(Args&&... args): base(std::forward<Args>(args)...) {}

    static constexpr def none            = 0x00;
    static constexpr def left_inclusive  = 0x01;
    static constexpr def right_inclusive = 0x02;
    static constexpr def dcsm            = 0x04;
    static constexpr def r2l             = 0x08;

    /// @var line_shift_flags::erm
    /// 消去の時 (abs(shift) >= p2 - p1 の時) に保護領域を消去しない事を表すフラグです。
    /// シフトの場合 (abs(shift) < p2 - p1 の時) には使われません。
    static constexpr def erm             = 0x10;
  };

  class line_t {
  private:
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
    mutable std::size_t m_prop_i;
    mutable curpos_t m_prop_x;

    std::uint32_t m_id = 0;
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
    std::vector<cell_t> const& cells() const { return m_cells; }
    line_attr_t& lflags() { return m_lflags; }
    line_attr_t const& lflags() const { return m_lflags; }
    curpos_t& home() { return m_home; }
    curpos_t const& home() const { return m_home; }
    curpos_t& limit() { return m_limit; }
    curpos_t const& limit() const { return m_limit; }

    bool has_protected_cells() const {
      return std::any_of(m_cells.begin(), m_cells.end(),
        [] (cell_t const& cell) { return cell.is_protected(); });
    }

    std::uint32_t version() const { return this->m_version; }
    std::uint32_t id() const { return m_id; }
    void set_id(std::uint32_t value) { this->m_id = value; }

  private:
    void _initialize_content(curpos_t width, attribute_t const& attr) {
      if (attr.is_default()) return;
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
    void clear(curpos_t width, attribute_t const& attr) {
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
    void clear_content(curpos_t width, attribute_t const& attr) {
      this->clear_content();
      this->_initialize_content(width, attr);
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
        if (x1 < x) return character_t::flag_wide_extension;
        return m_cells[i1].character;
      }
    }

  public:
    bool is_r2l(presentation_direction board_charpath) const {
      if (m_lflags & line_attr_t::is_character_path_ltor)
        return false;
      else if (m_lflags & line_attr_t::is_character_path_rtol)
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
    ///   変換元のx座標です。
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
    curpos_t to_data_position(curpos_t x, curpos_t width, presentation_direction board_charpath) const {
      return to_data_position(x, width, is_r2l(board_charpath));
    }
    curpos_t to_presentation_position(curpos_t x, curpos_t width, presentation_direction board_charpath) const {
      return to_presentation_position(x, width, is_r2l(board_charpath));
    }

  private:
    void _prop_cells_in_presentation(std::vector<cell_t>& buff, bool line_r2l) const;
  public:
    void get_cells_in_presentation(std::vector<cell_t>& buff, bool line_r2l) const;

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
    void _mono_compose_segments(line_segment_t const* comp, int count, curpos_t width, attribute_t const& fill_attr, bool line_r2l, bool dcsm);
    void _prop_compose_segments(line_segment_t const* comp, int count, curpos_t width, attribute_t const& fill_attr, bool line_r2l, bool dcsm);
  public:
    /// @fn void compose_segments(line_segment_t const* comp, int count, curpos_t width, bool line_r2l, attribute_t const& fill_attr);
    /// 表示部に於ける範囲を組み合わせて新しく行の内容を再構築します。
    void compose_segments(line_segment_t const* comp, int count, curpos_t width, attribute_t const& fill_attr, bool line_r2l, bool dcsm = false) {
      if (!m_prop_enabled)
        _mono_compose_segments(comp, count, width, fill_attr, line_r2l, dcsm);
      else
        _prop_compose_segments(comp, count, width, fill_attr, line_r2l, dcsm);
    }
    void truncate(curpos_t width, bool line_r2l, bool dcsm) {
      line_segment_t const seg = {0, width, line_segment_slice};
      compose_segments(&seg, 1, width, attribute_t {}, line_r2l, dcsm);
    }

  private:
    void _mono_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attribute_t const& fill_attr);
    void _bdsm_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attribute_t const& fill_attr);
    void _prop_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attribute_t const& fill_attr);
  public:
    void shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attribute_t const& fill_attr) {
      if (!m_prop_enabled)
        _mono_shift_cells(p1, p2, shift, flags, width, fill_attr);
      else if (!(flags & line_shift_flags::dcsm))
        _bdsm_shift_cells(p1, p2, shift, flags, width, fill_attr);
      else
        _prop_shift_cells(p1, p2, shift, flags, width, fill_attr);
    }

  public:
    void debug_string_nest(curpos_t width, presentation_direction board_charpath) const {
      bool const line_r2l = is_r2l(board_charpath);
      auto const& strings = this->update_strings(width, line_r2l);

      curpos_t x = 0;
      int parent = 0;
      std::fprintf(stderr, "[");
      for (std::size_t i = 1; i < strings.size(); i++) {
        auto const& range = strings[i];

        while (range.parent < parent) {
          int delta = strings[parent].end - x;
          if (delta) std::fprintf(stderr, "%d", delta);
          std::fprintf(stderr, "]"); // 抜ける
          x = strings[parent].end;
          parent = strings[parent].parent;
        }

        if (x < range.begin) {
          std::fprintf(stderr, "%d", range.begin - x);
          x = range.begin;
        }
        std::fprintf(stderr, "[");
        parent = i;
      }
      while (parent >= 0) {
        int delta = strings[parent].end - x;
        if (delta) std::fprintf(stderr, "%d", delta);
        std::fprintf(stderr, "]"); // 抜ける
        x = strings[parent].end;
        parent = strings[parent].parent;
      }
      std::putc('\n', stderr);
    }

  public:
    void debug_dump() const {
      for (cell_t const& cell : m_cells) {
        std::uint32_t code = cell.character.value;
        if (cell.character.is_marker()) {
          switch (code) {
          case character_t::marker_sds_l2r: std::fprintf(stderr, "\x1b[91mSDS(1)\x1b[m"); break;
          case character_t::marker_sds_r2l: std::fprintf(stderr, "\x1b[91mSDS(2)\x1b[m"); break;
          case character_t::marker_srs_beg: std::fprintf(stderr, "\x1b[91mSRS(1)\x1b[m"); break;
          case character_t::marker_sds_end: std::fprintf(stderr, "\x1b[91mSDS(0)\x1b[m"); break;
          case character_t::marker_srs_end: std::fprintf(stderr, "\x1b[91mSRS(1)\x1b[m"); break;
          default: break;
          }
        } else if (code == ascii_nul) {
          std::fprintf(stderr, "\x1b[37m@\x1b[m");
        } else {
          contra::encoding::put_u8(code, stderr);
        }
      }
      std::putc('\n', stderr);
    }
  };

  struct cursor_t {
  private:
    curpos_t m_x = 0, m_y = 0;
    bool m_xenl = false;

  public:
    attribute_t attribute;

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
    void set_x_keeping_xenl(curpos_t x, curpos_t width, curpos_t sll) {
      this->m_x = x;
      if (x == sll + 1 || x == width) return;
      this->m_xenl = false;
    }
  };

  struct board_t {
    contra::util::ring_buffer<line_t> m_lines;
    cursor_t cur;
    curpos_t m_width;
    curpos_t m_height;
    std::uint32_t m_line_count = 0;
    presentation_direction m_presentation_direction {presentation_direction_default};

    board_t(curpos_t width, curpos_t height): m_lines(height), m_width(width), m_height(height) {
      mwg_check(width > 0 && height > 0, "width = %d, height = %d", (int) width, (int) height);
      this->cur.set(0, 0);
      for (line_t& line : m_lines)
        line.set_id(m_line_count++);
    }
    board_t(): board_t(80, 32) {}

  public:
    void reset_size(curpos_t width, curpos_t height) {
      mwg_check(width > 0 && height > 0, "negative size is passed (width = %d, height = %d).", (int) width, (int) height);
      if (width == this->m_width && height == this->m_height) return;
      if (this->cur.x() >= width) cur.set_x(width - 1);
      if (this->cur.y() >= height) cur.set_y(height - 1);
      m_lines.resize(height);
      if (this->m_width > width) {
        for (auto& line : m_lines)
          line.truncate(width, false, true);
      }
      this->m_width = width;
      this->m_height = height;
    }

  public:
    curpos_t x() const { return cur.x(); }
    curpos_t y() const { return cur.y(); }

    line_t& line() { return m_lines[cur.y()]; }
    line_t const& line() const { return m_lines[cur.y()]; }
    line_t& line(curpos_t y) { return m_lines[y]; }
    line_t const& line(curpos_t y) const { return m_lines[y]; }

    curpos_t line_r2l(line_t const& line) const { return line.is_r2l(m_presentation_direction); }
    curpos_t line_r2l(curpos_t y) const { return line_r2l(m_lines[y]); }
    curpos_t line_r2l() const { return line_r2l(cur.y()); }

    curpos_t to_data_position(curpos_t y, curpos_t x) const {
      return m_lines[y].to_data_position(x, m_width, m_presentation_direction);
    }
    curpos_t to_presentation_position(curpos_t y, curpos_t x) const {
      return m_lines[y].to_presentation_position(x, m_width, m_presentation_direction);
    }

  private:
    void initialize_lines(curpos_t y1, curpos_t y2, attribute_t const& fill_attr) {
      for (curpos_t y = y1; y < y2; y++) {
        m_lines[y].clear(m_width, fill_attr);
        m_lines[y].set_id(m_line_count++);
      }
    }

  public:
    /// @fn void shift_lines(curpos_t y1, curpos_t y2, curpos_t count);
    /// 指定した範囲の行を前方または後方に移動します。
    /// @param[in] y1 範囲の開始位置を指定します。
    /// @param[in] y1 範囲の終端位置を指定します。
    /// @param[in] count 移動量を指定します。正の値を指定した時、
    ///   下方向にシフトします。負の値を指定した時、上方向にシフトします。
    void shift_lines(curpos_t y1, curpos_t y2, curpos_t count, attribute_t const& fill_attr) {
      y1 = contra::clamp(y1, 0, m_height);
      y2 = contra::clamp(y2, 0, m_height);
      if (y1 >= y2 || count == 0) return;
      if (y1 == 0 && y2 == m_height) {
        if (count < 0) {
          count = -count;
          if (count > m_height) count = m_height;
          // ToDo: 消える行を何処かに記録する?
          initialize_lines(0, count, fill_attr);
          m_lines.rotate(count);
        } else if (count > 0) {
          if (count > m_height) count = m_height;
          m_lines.rotate(m_height - count);
          initialize_lines(0, count, fill_attr);
        }
        return;
      }

      if (std::abs(count) >= y2 - y1) {
        initialize_lines(y1, y2, fill_attr);
      } else if (count > 0) {
        auto const it1 = m_lines.begin() + y1;
        auto const it2 = m_lines.begin() + y2;
        std::move_backward(it1, it2 - count, it2);
        initialize_lines(y1, y1 + count, fill_attr);
      } else {
        count = -count;
        auto const it1 = m_lines.begin() + y1;
        auto const it2 = m_lines.begin() + y2;
        std::move(it1 + count, it2, it1);
        initialize_lines(y2 - count, y2, fill_attr);
      }
    }
    void clear_screen() {
      // ToDo: 何処かに記録する
      for (auto& line : m_lines) {
        line.clear();
        line.set_id(m_line_count++);
      }
    }

  public:
    void debug_print(std::FILE* file) {
      for (auto const& line : m_lines) {
        if (line.lflags() & line_attr_t::is_line_used) {
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
