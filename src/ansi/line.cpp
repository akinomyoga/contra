#include "line.hpp"

using namespace contra::ansi;

void line_t::_mono_generic_replace_cells(curpos_t xL, curpos_t xR, cell_t const* cell, int count, int repeat, curpos_t width, int implicit_move) {
  this->m_version++;
  contra_unused(implicit_move);

  // 左側の中途半端な文字を消去
  curpos_t const ncell = (curpos_t) this->m_cells.size();
  if (xL < ncell && m_cells[xL].character.is_extension()) {
    for (std::ptrdiff_t q = xL - 1; q >= 0; q--) {
      bool const is_wide = this->m_cells[q].character.is_extension();
      this->m_cells[q].character = ascii_sp;
      this->m_cells[q].width = 1;
      if (!is_wide) break;
    }
  }

  // 右側の中途半端な文字を消去
  for (curpos_t q = xR; q < ncell && this->m_cells[q].character.is_extension(); q++) {
    this->m_cells[q].character = ascii_sp;
    this->m_cells[q].width = 1;
  }

  // 長さの調節
  curpos_t const tail_len = std::max(ncell - xR, 0);
  curpos_t const total_len = xL + width + tail_len;
  if (total_len > ncell) {
    cell_t fill;
    fill.character = ascii_nul;
    fill.attribute = 0;
    fill.width = 1;
    m_cells.insert(m_cells.end() - tail_len, total_len - ncell, fill);
  } else if (total_len < ncell) {
    m_cells.erase(m_cells.begin() + xL, m_cells.begin() + xL + (ncell - total_len));
  }

  // 文字を書き込む
  curpos_t x = xL;
  for (int r = 0; r < repeat; r++) {
    for (int i = 0; i < count; i++) {
      curpos_t const w = cell[i].width;
      m_cells[x] = cell[i];
      for (curpos_t i = 1; i < w; i++) {
        m_cells[x + i].character = character_t::flag_wide_extension;
        m_cells[x + i].attribute = cell[i].attribute;
        m_cells[x + i].width = 0;
      }
      x += w;
    }
  }
}

void line_t::_prop_generic_replace_cells(curpos_t xL, curpos_t xR, cell_t const* cell, int count, int repeat, curpos_t width, int implicit_move) {
  std::size_t const ncell = m_cells.size();

  // 置き換える範囲 i1..i2 を決定する。
  // Note: ゼロ幅の文字は "mark 文字 cluster" の様に文字の周りに分布している。
  //   暗黙移動の方向の境界上の zw body (marker 等) は削除の対象とする。
  std::size_t i1, i2;
  curpos_t x1, x2;
  std::tie(i1, x1) = _prop_glb(xL, implicit_move < 0);
  std::tie(i2, x2) = _prop_lub(xR, implicit_move > 0);
  if (i1 > i2) i2 = i1; // cell.width=0 implicit_move=0 の時

  // 書き込み文字数の計算
  std::size_t nwrite = count * repeat;
  if (x1 < xL) nwrite += xL - x1;
  if (xR < x2) nwrite += x2 - xR;

  // fill に使う文字を元の内容を元に決定しておく。
  cell_t lfill, rfill;
  if (x1 < xL) {
    // Note: x1 < xL && i1 < ncell の時点で x1 < xL <= xR <= x2 より i1 < i2 は保証される。
    // Note: 文字幅 2 以上の NUL が設置されている事があるか分からないが、その時には NUL で埋める。
    if (i1 < ncell) mwg_assert(i1 < i2);
    lfill.character = i1 < ncell && m_cells[i1].character.value != ascii_nul ? ascii_sp : ascii_nul;
    lfill.attribute = i1 < i2 ? m_cells[i1].attribute : 0;
    lfill.width = 1;
  }
  if (xR < x2) {
    // Note: xR < x2 の時点で x1 <= xL <= xR < x2 なので i1 < i2 が保証される。
    //   更に、i1 < i2 <= ncell も保証される。
    // Note: 文字幅 2 以上の NUL が設置されている事があるか分からないが、その時には NUL で埋める。
    mwg_assert(i1 < i2 && i1 < ncell);
    rfill.character = m_cells[i2 - 1].character.value != ascii_nul ? ascii_sp : ascii_nul;
    rfill.attribute = m_cells[i2 - 1].attribute;
    rfill.width = 1;
  }

  // 書き込み領域の確保
  this->m_version++;
  if (nwrite < i2 - i1) {
    m_cells.erase(m_cells.begin() + (i1 + nwrite), m_cells.begin() + i2);
  } else if (nwrite > i2 - i1) {
    m_cells.insert(m_cells.begin() + i2, i1 + nwrite - i2, rfill);
  }

  // m_prop_i, m_prop_x 更新
  m_prop_i = i1 + nwrite;
  m_prop_x = xL + width + std::max(0, x2 - xR);

  // 余白と文字の書き込み
  curpos_t p = i1;
  while (x1++ < xL) m_cells[p++] = lfill;
  for (int r = 0; r < repeat; r++)
    for (int i = 0; i < count; i++)
      m_cells[p++] = cell[i];
  while (xR < x2--) m_cells[p++] = rfill;
}

void line_t::_prop_reverse(curpos_t width) {
  // 幅を width に強制
  std::size_t iN;
  curpos_t xN;
  std::tie(iN, xN) = _prop_glb(width, false);
  if (iN < m_cells.size()) m_cells.erase(m_cells.begin() + iN, m_cells.end());
  if (xN < width) m_cells.insert(m_cells.end(), width - xN, cell_t(ascii_nul));


  std::vector<cell_t> buff;
  buff.reserve(m_cells.size() * 2);

  struct elem_t {
    std::uint32_t beg_marker;
    std::uint32_t end_marker;
  };
  std::vector<elem_t> stack;

  auto _push = [&] (cell_t& cell, std::uint32_t beg_marker, std::uint32_t end_marker) {
    cell.character = end_marker;
    elem_t elem;
    elem.beg_marker = beg_marker;
    elem.end_marker = end_marker;
    stack.emplace_back(std::move(elem));
  };

  auto _flush = [&] () {
    while (stack.size()) {
      buff.emplace_back(stack.back().beg_marker);
      stack.pop_back();
    }
  };

  for (std::size_t i = 0; i < m_cells.size(); ) {
    cell_t& cell = m_cells[i];
    std::uint32_t const code = cell.character.value;
    bool remove = false;
    if (cell.character.is_marker()) {
      switch (code) {
      case character_t::marker_sds_l2r: _push(cell, code, character_t::marker_sds_end); break;
      case character_t::marker_sds_r2l: _push(cell, code, character_t::marker_sds_end); break;
      case character_t::marker_srs_beg: _push(cell, code, character_t::marker_srs_end); break;
      case character_t::marker_sds_end:
      case character_t::marker_srs_end:
        remove = true; // 対応する始まりがもし見つからなければ削除
        while (stack.size()) {
          if (stack.back().end_marker == code) {
            cell.character = stack.back().beg_marker;
            remove = false;
            stack.pop_back();
            break;
          } else {
            buff.emplace_back(stack.back().beg_marker);
            stack.pop_back();
          }
        }
        break;
      default: break;
      }
    } else {
      if (cell.character.value == ascii_nul) _flush();
    }

    std::size_t i1 = i++;
    while (i < m_cells.size() && m_cells[i].character.is_extension()) i++;
    if (!remove)
      for (std::size_t p = i; p-- > i1; )
        buff.emplace_back(std::move(m_cells[p]));
  }
  _flush();

  std::reverse(buff.begin(), buff.end());

  m_cells.swap(buff);
  m_version++;
  m_prop_i = 0;
  m_prop_x = 0;
}

std::vector<line_t::nested_string> const& line_t::update_strings(curpos_t width, bool line_r2l) const {
  std::vector<nested_string>& ret = m_strings_cache;
  if (m_strings_version == m_version && m_strings_r2l == line_r2l) return ret;
  ret.clear();

  curpos_t x1 = 0;
  int istr = -1;
  auto _push = [&] (std::uint32_t beg_marker, std::uint32_t end_marker, bool new_r2l) {
    nested_string str;
    str.begin = x1;
    str.end = -1;
    str.r2l = new_r2l;
    str.beg_marker = beg_marker;
    str.end_marker = end_marker;
    str.parent = istr;
    istr = ret.size();
    ret.emplace_back(str);
  };

  _push(0, 0, line_r2l);
  ret[0].end = width;
  m_strings_version = m_version;
  m_strings_r2l = line_r2l;

  if (!m_prop_enabled) return ret;

  for (cell_t const& cell : m_cells) {
    std::uint32_t code = cell.character.value;
    if (cell.character.is_marker()) {
      switch (code) {
      case character_t::marker_sds_l2r: _push(code, character_t::marker_sds_end, false); break;
      case character_t::marker_sds_r2l: _push(code, character_t::marker_sds_end, true); break;
      case character_t::marker_srs_beg: _push(code, character_t::marker_srs_end, !ret[istr].r2l); break;
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
    } else {
      if (cell.character.value == ascii_nul) {
        while (istr >= 1) {
          ret[istr].end = x1;
          istr = ret[istr].parent;
        }
      }
      x1 += cell.width;
    }
  }
  while (istr >= 1) {
    ret[istr].end = x1;
    istr = ret[istr].parent;
  }

  return ret;
}

curpos_t line_t::_prop_to_presentation_position(curpos_t x, curpos_t width, bool line_r2l) const {
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

void line_t::calculate_data_ranges_from_presentation_range(slice_ranges_t& ret, curpos_t x1, curpos_t x2, curpos_t width, bool line_r2l) const {
  ret.clear();
  auto _register = [&ret] (curpos_t x1, curpos_t x2) {
    if (ret.size() && ret.back().second == x1)
      ret.back().second = x2;
    else
      ret.emplace_back(std::make_pair(x1, x2));
  };

  if (!m_prop_enabled) {
    if (line_r2l)
      _register(width - x2, width - x1);
    else
      _register(x1, x2);
    return;
  }

  auto const& strings = this->update_strings(width, line_r2l);

  struct elem_t { curpos_t x1, x2; bool r2l; int parent; };
  std::vector<elem_t> stack;

  bool r2l = false;
  if (line_r2l) {
    x1 = width - x1;
    x2 = width - x2;
    std::swap(x1, x2);
    r2l = line_r2l;
  }
  mwg_check(strings.size() <= std::numeric_limits<int>::max());
  for (std::size_t i = 1; i < strings.size(); i++) {
    auto const& str = strings[i];

    while (stack.size() && stack.back().parent > str.parent) {
      if (x1 >= 0) _register(x1, x2);
      x1 = stack.back().x1;
      x2 = stack.back().x2;
      r2l = stack.back().r2l;
      stack.pop_back();
    }

    if (x1 < 0 || str.end < x1) continue;

    // (x1...x2) [beg...end]
    if (x2 < str.begin) {
      _register(x1, x2);
      x1 = -1;
      if(stack.empty()) break;
      continue;
    }

    // (x1...[beg x2) ... end]
    if (x1 <= str.begin) {
      _register(x1, str.begin);
      x1 = str.begin;
    }

    // [beg (x1... end] ... x2)
    if (str.end <= x2) {
      stack.emplace_back(elem_t({str.end, x2, r2l, (int) i}));
      x2 = str.end;
    }

    // [beg (x1...x2) end]
    if (str.r2l != r2l) {
      x1 = str.begin + str.end - x1;
      x2 = str.begin + str.end - x2;
      std::swap(x1, x2);
      r2l = str.r2l;
    }
  }

  if (x1 >= 0) _register(x1, x2);
  while (stack.size()) {
    _register(stack.back().x1, stack.back().x2);
    stack.pop_back();
  }
}

void line_t::compose_segments(line_segment_t const* comp, int count, curpos_t width, bool line_r2l) {
  cell_t fill;
  fill.character = ascii_nul;
  fill.attribute = 0;
  fill.width = 1;

  cell_t mark;
  mark.attribute = 0;
  mark.width = 0;

  std::vector<cell_t> cells;
  auto _segment = [&,this] (curpos_t xL, curpos_t xR) {
    std::size_t i1, i2;
    curpos_t x1, x2;
    std::tie(i1, x1) = this->_prop_glb(xL, true);
    std::tie(i2, x2) = this->_prop_lub(xR, true);
    if (x1 < xL) i1++;
    if (xR < x2) i2--;
    if (i1 > i2) {
      fill.character = i2 < m_cells.size() && m_cells[i2].character.value != ascii_nul ? ascii_sp : ascii_nul;
      cells.insert(cells.end(), xR - xL, fill);
    } else {
      if (x1 < xL) {
        fill.character = m_cells[i1 - 1].character.value == ascii_nul ? ascii_nul : ascii_sp;
        cells.insert(cells.end(), xL - x1, fill);
      }
      cells.insert(cells.end(), m_cells.begin() + i1, m_cells.begin() + i2);
      if (xR < x2) {
        fill.character = m_cells[i2].character.value == ascii_nul ? ascii_nul : ascii_sp;
        cells.insert(cells.end(), x2 - xR, fill);
      }
    }
  };

  slice_ranges_t ranges;
  auto const& strings = this->update_strings(width, line_r2l);

  auto _process_segment = [&] (line_segment_t const& seg) {
    curpos_t const p1 = seg.p1;
    curpos_t const p2 = seg.p2;
    if (p1 >= p2) return;
    switch (seg.type) {
    case line_segment_slice:
      calculate_data_ranges_from_presentation_range(ranges, p1, p2, width, line_r2l);
      if (ranges.size()) {
        std::size_t istr;

        // 文字列再開
        std::size_t const insert_index = cells.size();
        istr = find_innermost_string(ranges[0].first, true, width, line_r2l);
        while (istr) {
          mark.character = strings[istr].beg_marker;
          cells.insert(cells.begin() + insert_index, mark);
          istr = strings[istr].parent;
        }

        for (auto const& range : ranges)
          _segment(range.first, range.second);

        // 文字列終端
        istr = find_innermost_string(ranges.back().second, false, width, line_r2l);
        while (istr) {
          mark.character = strings[istr].end_marker;
          cells.push_back(mark);
          istr = strings[istr].parent;
        }
      }
      break;
    case line_segment_fill:
      fill.character = ascii_nul;
      cells.insert(cells.end(), p2 - p1, fill);
      break;
    case line_segment_space:
      fill.character = ascii_sp;
      cells.insert(cells.end(), p2 - p1, fill);
      break;
    }
  };

  if (line_r2l) {
    for (int i = count; i--; )
      _process_segment(comp[i]);
  } else {
    for (int i = 0; i < count; i++)
      _process_segment(comp[i]);
  }

  m_cells.swap(cells);
  m_version++;
  m_prop_i = 0;
  m_prop_x = 0;
}
