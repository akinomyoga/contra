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

  void board_line::update_markers_on_overwriting(curpos_t beg, curpos_t end, bool simd) {
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

}
