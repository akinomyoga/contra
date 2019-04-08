#include "board.h"

namespace contra {

  static constexpr bool is_empty_string(line_marker const& m1, line_marker const& m2) {
    if (m1.position != m2.position) return false;
    if (is_string_aligned(m2.stype))
      return is_string_bidi_begin(m1.stype);
    else
      return is_string_corresponding_pair(m1.stype, m2.stype);
  }

  static std::ptrdiff_t find_next_string_aligned(std::vector<line_marker> const& markers, std::ptrdiff_t beg, std::ptrdiff_t end = -1) {
    if (end < 0) end = markers.size();
    for (std::ptrdiff_t j = beg + 1; j < end; j++)
      if (is_string_aligned(markers[j].stype)) return j;
    return -1;
  }

  void board_line::update_markers_on_overwrite(curpos_t beg, curpos_t end, bool simd) {
    std::vector<line_marker>::iterator const it0 = m_markers.begin();
    std::ptrdiff_t const ibeg = std::lower_bound(it0, m_markers.end(), beg,
      [] (line_marker& m, curpos_t pos) { return m.position < pos; }) - it0;
    std::ptrdiff_t const iend = std::upper_bound(it0, m_markers.end(), end,
      [] (curpos_t pos, line_marker& m) { return pos < m.position; }) - it0;

    constexpr nested_string_type removeMark = string_unknown;
    bool toRemove = false, flag_changed = false;

    for (std::ptrdiff_t i = ibeg; i < iend; i++) {
      line_marker& m = m_markers[i];
      if (is_string_bidi(m.stype)) {
        if (beg < m.position && m.position < end) {
          m.position = simd ? beg : end;
          flag_changed = true;

          if (!simd && is_string_bidi_begin(m.stype)) {
            // SDS/SRS 開始直後に aligned string 開始/終了がある場合は空文字列として削除
            if (find_next_string_aligned(m_markers, i, iend) > 0) {
              m.stype = removeMark;
              toRemove = true;
              continue;
            }
          }
        }

        if (is_string_bidi_end(m.stype)) {
          if (i == 0 || is_string_aligned(m_markers[i - 1].stype)) {
            // 行頭または aligned string 先頭に SDS/SRS 終端があるので迷子マーカとして削除
            m.stype = removeMark;
            toRemove = true;
          } else if (is_empty_string(m_markers[i - 1], m)) {
            // SDS/SRS 開始直後に終了 SDS/SRS があるので空文字列として削除
            m.stype = m_markers[i - 1].stype = removeMark;
            toRemove = true;
          }
        }

      } else if (is_string_aligned(m.stype)) {
        if ((beg < m.position && m.position < end) || m.position == (simd ? beg : end)) {
          m.stype = removeMark;
          toRemove = true;

          if (m.stype != string_aligned_end) {
            std::ptrdiff_t const j = find_next_string_aligned(m_markers, i);
            if (j >=0 && m_markers[j].stype == string_aligned_end) {
              m_markers[j].stype = removeMark;
              toRemove = true;
            }
          }
        }
      } else
        mwg_check("BUG: unexpected type of marker");
    }

    if (toRemove) {
      m_markers.erase(
        std::remove_if(m_markers.begin() + ibeg, m_markers.end(),
          [] (line_marker const& m) { return m.stype == removeMark; }),
        m_markers.end());
    }
    if (toRemove || flag_changed)
      m_strings_updated = false;
  }

  std::vector<nested_string> const& board_line::get_nested_strings() const {
    if (m_strings_updated) return m_strings_cached;
    m_strings_updated = true;

    std::vector<nested_string>& strings = m_strings_cached;
    strings.clear();

    std::vector<std::size_t> nest;
    for (line_marker const marker : m_markers) {
      switch (marker.stype) {
      case string_directed_charpath:
      case string_directed_rtol:
      case string_directed_ltor:
      case string_reversed:
        goto begin_string;

      case string_directed_end:
      case string_reversed_end:
        {
          nested_string_type stype;
          while (nest.size() && is_string_bidi(stype = strings[nest.back()].stype)) {
            strings[nest.back()].end = marker.position;
            nest.pop_back();
            if ((stype == string_reversed) == (marker.stype == string_reversed_end)) break;
          }
          break;
        }

      case string_aligned_left:
      case string_aligned_right:
      case string_aligned_centered:
      case string_aligned_char:
      case string_aligned_end:
        for (std::size_t index : nest)
          strings[index].end = marker.position;
        nest.clear();
        if (marker.stype != string_aligned_end)
          goto begin_string;
        break;

      default:
        mwg_assert(0, "BUG");
        break;

      begin_string:
        {
          nested_string str;
          str.begin = marker.position;
          str.end   = nested_string::npos;
          str.stype = marker.stype;

          std::size_t const index = strings.size();
          nest.push_back(index);
          strings.push_back(str);
          break;
        }
      }
    }

    return strings;
  }

  void board_line::set_nested_strings(std::vector<nested_string>&& strings) {
    this->m_markers.clear();
    this->m_markers.reserve(strings.size() * 2);

    std::vector<line_marker> endMarkers;
    endMarkers.reserve(strings.size());
    for (std::size_t i = 0, iN = strings.size(); i < iN; i++) {
      nested_string const& str = strings[i];
      mwg_check(
        str.begin < str.end,
        "invalid_argument 'strings': strings[%zu] has non-positive range %d..%d",
        i, (int) str.begin, (int) str.end);
      mwg_check(
        !is_string_end(str.stype),
        "invalid_argument 'strings': strings[%zu].stype (%d) is the end of string",
        i, (int) str.stype);

      {
        line_marker bmark;
        bmark.position = str.begin;
        bmark.stype = str.stype;

        while (endMarkers.size() &&
          endMarkers.back().position != nested_string::npos &&
          endMarkers.back().position <= bmark.position
        ) {
          /* Aligned strings clear all the existing nesting levels because
           * they cannot be nested, so the end markers can be omitted.
           * 省略せずに明示的に終端マーカを設置しても問題はないが省略する。
           */
          bool const toOmit = is_string_aligned(bmark.stype);

          if (!toOmit) this->m_markers.push_back(endMarkers.back());
          endMarkers.pop_back();
        }

        mwg_check(
          this->m_markers.size() == 0 ||
          this->m_markers.back().position <= bmark.position,
          "invalid_argument 'string': 'strings' is not properly sorted at strings[%zu]", i);

        this->m_markers.push_back(bmark);
      }

      {
        line_marker emark;
        emark.position = str.end;
        emark.stype =
          is_string_directed_begin(str.stype) ? string_directed_end :
          is_string_reversed_begin(str.stype) ? string_reversed_end :
          is_string_aligned_begin(str.stype) ? string_aligned_end :
          string_unknown;

        mwg_check(
          endMarkers.size() == 0 ||
          endMarkers.back().position == nested_string::npos ||
          emark.position <= endMarkers.back().position,
          "invalid_argument 'strings': strings[%zu] overruns the parent-string end", i);

        mwg_check(
          emark.stype != string_unknown,
          "invalid_argument 'strings': strings[%zu].stype has unrecognized value %d", i, (int) str.stype);

        endMarkers.push_back(emark);
      }
    }

    for (std::size_t i = endMarkers.size(); i--; )
      this->m_markers.push_back(endMarkers[i]);

    this->m_strings_cached = std::move(strings);
    this->m_strings_updated = true;
  }

  /*---------------------------------------------------------------------------
   *
   * void board_line::update_markers_on_erase(curpos_t beg, curpos_t end);
   *
   *---------------------------------------------------------------------------
   *
   * 動作の詳細に関しては tty_player.cpp (do_ech) を参照のこと
   *
   * 開いている文字列を閉じなければならないので入れ子状態を追跡しなければならない。
   * update_string_nest はその為の関数である。
   *
   */

  template<typename ContinuePredicate>
  static void update_string_nest(
    std::vector<line_marker const*>& nest,
    std::vector<line_marker>::const_iterator& it,
    ContinuePredicate fcond
  ) {
    for (; fcond(); ++it) {
      line_marker const& m = *it;
      switch (m.stype) {
      case string_directed_charpath:
      case string_directed_rtol:
      case string_directed_ltor:
      case string_reversed:
        nest.push_back(&m);
        break;

      case string_directed_end:
      case string_reversed_end:
        {
          nested_string_type stype;
          while (nest.size() && is_string_bidi(stype = nest.back()->stype)) {
            nest.pop_back();
            if ((stype == string_reversed) == (m.stype == string_reversed_end)) break;
          }
        }
        break;

      case string_aligned_left:
      case string_aligned_right:
      case string_aligned_centered:
      case string_aligned_char:
      case string_aligned_end:
        nest.clear();
        if (m.stype != string_aligned_end)
          nest.push_back(&m);
        break;

      default:
        mwg_check(0, "BUG: invalid stype");
      }
    }
  }

  void board_line::update_markers_on_erase(curpos_t beg, curpos_t end) {
    if (beg >= end || m_markers.size() == 0) return;

    typedef std::vector<line_marker>::const_iterator it_t;
    it_t const it0 = m_markers.begin(), itN = m_markers.end();

    it_t it = it0;
    std::vector<line_marker const*> nest;
    update_string_nest(nest, it, [=, &it] () { return it != itN && it->position < beg; });
    std::vector<line_marker> markers2(it0, it);
    if (nest.size()) {
      std::ptrdiff_t const ibeg = it - it0;
      if (is_string_aligned_begin(nest[0]->stype))
        markers2.push_back(line_marker { ibeg, string_aligned_end });
      else {
        for (std::size_t i = nest.size(); i--; ) {
          nested_string_type const stype = nest[i]->stype;
          if (is_string_directed_begin(stype))
            markers2.push_back(line_marker { ibeg, string_directed_end });
          else if (is_string_reversed_begin(stype))
            markers2.push_back(line_marker { ibeg, string_reversed_end });
          else if (is_string_aligned_begin(stype)) {
            mwg_assert(i == 0, "aligned string should be the top level");
            markers2.push_back(line_marker { ibeg, string_aligned_end });
          } else
            mwg_assert(0, "BUG: invalid stype");
        }
      }
    }

    update_string_nest(nest, it, [=, &it] () { return it != itN && it->position <= end; });
    if (nest.size()) {
      std::ptrdiff_t const iend = it - it0;
      for (line_marker const* m : nest)
        markers2.push_back(line_marker { iend, m->stype });
    }

    markers2.insert(markers2.end(), it, itN);

    m_markers.swap(markers2);
  }

}
