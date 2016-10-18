#include "board.h"

namespace contra {
  const char* get_ascii_name(char32_t value) {
    static const char* c0names[32] = {
      "NUL", "SOH", "STK", "ETX", "EOT", "ENQ", "ACK", "BEL",
      "BS" , "HT" , "LF" , "VT" , "FF" , "CR" , "SO" , "SI" ,
      "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
      "CAN", "EM ", "SUB", "ESC", "FS" , "GS" , "RS" , "US" ,
    };
    static const char* c1names[32] = {
      "PAD" , "HOP" , "BPH" , "NBH" , "IND" , "NEL" , "SSA" , "ESA" ,
      "HTS" , "HTJ" , "VTS" , "PLD" , "PLU" , "RI"  , "SS2" , "SS3" ,
      "DCS" , "PU1" , "PU2" , "STS" , "CCH" , "MW"  , "SPA" , "EPA" ,
      "SOS" , "SGCI", "SCI" , "CSI" , "ST"  , "OSC" , "PM"  , "APC" ,
    };

    if (value < 0x20)
      return c0names[value];
    else if (0x80 <= value && value < 0xA0)
      return c1names[value - 0x80];
    else if (value == 0x20)
      return "SP";
    else if (value == 0x7F)
      return "DEL";
    else
      return nullptr;
  }

  static constexpr bool is_empty_string(line_marker const& m1, line_marker const& m2) {
    if (m1.position != m2.position) return false;
    if (is_string_aligned(m2.stype))
      return is_string_bidi_begin(m1.stype);
    else
      return is_string_corresponding_pair(m1.stype, m2.stype);
  }

  void board_line::update_markers_on_overwrite(curpos_t beg, curpos_t end, bool simd) {
    std::vector<line_marker>::iterator const it0 = m_markers.begin();
    std::ptrdiff_t const ibeg = std::lower_bound(it0, m_markers.end(), beg,
      [](line_marker& m, curpos_t pos){ return m.position < pos;}) - it0;
    std::ptrdiff_t const iend = std::upper_bound(it0, m_markers.end(), end,
      [](curpos_t pos, line_marker& m){ return pos < m.position;}) - it0;

    constexpr nested_string_type removeMark = string_unknown;
    bool toRemove = false;

    for (std::ptrdiff_t i = ibeg; i < iend; i++) {
      line_marker& m = m_markers[i];
      if (is_string_bidi(m.stype)) {
        if (beg < m.position && m.position < end) {
          m.position = simd? beg: end;

          if (!simd && is_string_bidi_begin(m.stype)) {
            // SDS/SRS 開始直後に aligned string 開始/終了がある場合は空文字列として削除
            for (std::ptrdiff_t j = i + 1; j < iend; i++) {
              if (is_string_aligned(m_markers[j].stype)) {
                m.stype = removeMark;
                break;
              }
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
        if ((beg < m.position && m.position < end) || m.position == (simd? beg: end)) {
          m.stype = removeMark;
          toRemove = true;

          if (m.stype == string_aligned_end) break;

          for (std::ptrdiff_t j = i + 1, jN = m_markers.size(); j < jN; j++) {
            if (!is_string_aligned(m_markers[j].stype)) continue;
            if (m_markers[j].stype == string_aligned_end) {
              m_markers[j].stype = removeMark;
              toRemove = true;
            }
            break;
          }
        }
      } else
        mwg_check("BUG: unexpected type of marker");
    }

    if (toRemove) {
      std::ptrdiff_t idst = ibeg;
      for (std::ptrdiff_t isrc = ibeg, iN = m_markers.size(); isrc < iN; isrc++) {
        if (m_markers[isrc].stype == removeMark) continue;
        if (idst != isrc) m_markers[idst] = m_markers[isrc];
        idst++;
      }
      m_markers.erase(m_markers.begin() + idst, m_markers.end());
    }
  }

  std::vector<nested_string> const& board_line::get_nested_strings() const {
    if (m_strings_updated) return m_strings_cached;
    m_strings_updated = true;

    std::vector<nested_string>& strings = m_strings_cached;
    strings.clear();

    std::vector<std::size_t> nest;
    for (line_marker const marker: m_markers) {
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
        for (std::size_t index: nest)
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

}
