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
      for (curpos_t j = 1; j < w; j++) {
        m_cells[x + j].character = character_t::flag_wide_extension;
        m_cells[x + j].attribute = cell[i].attribute;
        m_cells[x + j].width = 0;
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

curpos_t line_t::convert_position(bool toPresentationPosition, curpos_t srcX, int edge_type, curpos_t width, bool line_r2l) const {
  if (!m_prop_enabled) return srcX;
  std::vector<nested_string> const& strings = this->update_strings(width, line_r2l);
  if (strings.empty()) return srcX;

  if (edge_type > 0) srcX--;

  bool r2l = line_r2l;
  curpos_t x = toPresentationPosition ? 0 : srcX;
  for (nested_string const& range : strings) {
    curpos_t const referenceX = toPresentationPosition ? srcX : x;
    if (referenceX < range.begin) break;
    if (referenceX < range.end && range.r2l != r2l) {
      x = range.end - 1 - (x - range.begin);
      r2l = range.r2l;
      edge_type = -edge_type;
    }
  }

  if (toPresentationPosition) {
    x = srcX - x;
    if (r2l != line_r2l) x = -x;
  }

  if (edge_type > 0) x++;

  return x;
}

curpos_t line_t::_prop_to_presentation_position(curpos_t x, bool line_r2l) const {
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
      return a;
  }

  while (stack.size()) _pop();
  if (!contains) a = x;
  return a;
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
    _register(x1, x2);
    return;
  }

  auto const& strings = this->update_strings(width, line_r2l);

  struct elem_t { curpos_t x1, x2; bool r2l; int parent; };
  std::vector<elem_t> stack;

  bool r2l = line_r2l;
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

void line_t::_mono_compose_segments(line_segment_t const* comp, int count, curpos_t width, attribute_t const& fill_attr) {
  struct slice_t {
    curpos_t p1, p2;
    curpos_t shift;
  };
  std::vector<slice_t> slices;
  slices.reserve((std::size_t) count);

  curpos_t x = 0;
  for (int i = 0; i < count; i++) {
    line_segment_t const& seg  = comp[i];
    int const type = seg.type;
    curpos_t const p1 = seg.p1;
    curpos_t const p2 = std::min({seg.p2, width, seg.p1 + width - x});
    curpos_t const delta = p2 - p1;
    if (delta < 0) break;
    if (delta == 0) continue;

    if (type == line_segment_slice)
      slices.emplace_back(slice_t({p1, p2, x - p1}));
    x += delta;
  }
  mwg_check(m_cells.size() <= (std::size_t) width);

  cell_t fill;
  fill.character = ascii_nul;
  fill.attribute = 0;
  fill.width = 1;
  m_cells.resize(x, fill);
  fill.attribute = fill_attr; // 後で使う

  // 境界上の wide 文字をスペースに置き換える関数
  auto _erase_wide_on_boundary = [this] (curpos_t index) {
    if (!m_cells[index].character.is_wide_extension()) return;
    curpos_t l = index, u = index + 1;
    if (l > 0) l--;
    while (l > 0 && m_cells[l].character.is_wide_extension()) l--;
    while (u < (curpos_t) m_cells.size() && m_cells[u].character.is_wide_extension()) u++;
    for (; l < u; l++) {
      m_cells[l].character = ascii_sp;
      m_cells[l].width = 1;
    }
  };

  // 移動できる slice から順に移動する
  auto it0 = m_cells.begin();
  for (std::size_t i0 = 0, i = 0; i < slices.size(); i++) {
    slice_t const& s = slices[i];

    // 境界上の全角文字の処理
    _erase_wide_on_boundary(s.p1);
    if (s.p2 < (curpos_t) m_cells.size()) _erase_wide_on_boundary(s.p2);

    if (s.shift < 0) {
      std::move(it0 + s.p1, it0 + s.p2, it0 + s.p1 + s.shift);
    } else if (s.shift > 0) {
      curpos_t const dst = s.p2 + s.shift;
      if (i + 1 < slices.size() && slices[i + 1].p1 < dst) continue;
      if (s.p1 < s.p2) std::move_backward(it0 + s.p1, it0 + s.p2, it0 + dst);
      for (std::size_t j = i; j-- >= i0; ) {
        slice_t const& t = slices[j];
        if (t.p1 < t.p2)
          std::move_backward(it0 + t.p1, it0 + t.p2, it0 + t.p2 + t.shift);
      }
    }
    i0 = i + 1;
  }

  // 余白を埋める
  x = 0;
  for (int i = 0; i < count; i++) {
    line_segment_t const& seg  = comp[i];
    int const type = seg.type;
    curpos_t const p1 = seg.p1;
    curpos_t const p2 = std::min({seg.p2, width, seg.p1 + width - x});
    curpos_t const delta = p2 - p1;
    if (delta < 0) break;
    if (delta == 0) continue;

    switch (type) {
    case line_segment_erase:
      fill.character = ascii_nul;
      for (curpos_t p = x, pN = x + delta; p < pN; p++) m_cells[p] = fill;
      break;
    case line_segment_space:
      fill.character = ascii_sp;
      for (curpos_t p = x, pN = x + delta; p < pN; p++) m_cells[p] = fill;
      break;
    case line_segment_erase_unprotected:
      fill.character = ascii_nul;
      for (curpos_t p = x, pN = x + delta; p < pN; p++)
        if (!m_cells[p].is_protected())
          m_cells[p] = fill;
      break;
    case line_segment_slice: break;
    }
    x += delta;
  }

  m_version++;
}

void line_t::_prop_compose_segments(line_segment_t const* comp, int count, curpos_t width, attribute_t const& fill_attr, bool line_r2l, bool dcsm) {
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
    // Note: 境界上の wide 文字を跨がない様に lub/glb を逆に使っている
    std::tie(i1, x1) = this->_prop_lub(xL, false);
    std::tie(i2, x2) = this->_prop_glb(xR, false);
    if (i1 > i2) {
      // Note: 或る wide 文字の内部にいる時にここに来る。
      fill.character = i2 < m_cells.size() && m_cells[i2].character.value != ascii_nul ? ascii_sp : ascii_nul;
      fill.attribute = 0;
      cells.insert(cells.end(), xR - xL, fill);
    } else {
      if (xL < x1) {
        mwg_assert(i1 > 0);
        fill.character = m_cells[i1 - 1].character.value == ascii_nul ? ascii_nul : ascii_sp;
        fill.attribute = m_cells[i1 - 1].attribute;
        cells.insert(cells.end(), x1 - xL, fill);
      }
      cells.insert(cells.end(), m_cells.begin() + i1, m_cells.begin() + i2);
      if (x2 < xR) {
        if (i2 < m_cells.size()) {
          fill.character = m_cells[i2].character.value == ascii_nul ? ascii_nul : ascii_sp;
          fill.attribute = m_cells[i2].attribute;
        } else {
          fill.character = ascii_nul;
          fill.attribute = 0;
        }
        cells.insert(cells.end(), xR - x2, fill);
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
      if (dcsm) {
        ranges.clear();
        ranges.emplace_back(std::make_pair(p1, p2));
      } else
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
          if (cells.size() && cells.back().character.value == strings[istr].beg_marker) {
            cells.pop_back();
          } else {
            mark.character = strings[istr].end_marker;
            cells.push_back(mark);
          }
          istr = strings[istr].parent;
        }
      }
      break;
    case line_segment_erase:
      fill.character = ascii_nul;
      fill.attribute = fill_attr;
      cells.insert(cells.end(), p2 - p1, fill);
      break;
    case line_segment_space:
      fill.character = ascii_sp;
      fill.attribute = fill_attr;
      cells.insert(cells.end(), p2 - p1, fill);
      break;

    case line_segment_erase_unprotected:
      if (dcsm) {
        ranges.clear();
        ranges.emplace_back(std::make_pair(p1, p2));
      } else
        calculate_data_ranges_from_presentation_range(ranges, p1, p2, width, line_r2l);

      fill.character = ascii_nul;
      fill.attribute = fill_attr;
      if (ranges.size() != 0) {
        // 左右境界上の零幅文字は既に左右に取り込まれている筈なので無視。
        std::vector<curpos_t> skip_boundaries;
        {
          std::vector<curpos_t> left_boundaries, right_boundaries;
          slice_ranges_t ranges_end;
          calculate_data_ranges_from_presentation_range(ranges_end, p1, p1, width, line_r2l);
          for (auto const& range : ranges_end) left_boundaries.push_back(range.first);
          calculate_data_ranges_from_presentation_range(ranges_end, p2, p2, width, line_r2l);
          for (auto const& range : ranges_end) right_boundaries.push_back(range.first);
          skip_boundaries.resize(left_boundaries.size() + right_boundaries.size());
          std::merge(left_boundaries.begin(), left_boundaries.end(),
            right_boundaries.begin(), right_boundaries.end(), skip_boundaries.begin());
        }
        auto _to_skip = [&skip_boundaries] (curpos_t const x) {
          return std::lower_bound(skip_boundaries.begin(), skip_boundaries.end(), x) != skip_boundaries.end();
        };

        // protected なセルを拾いながら空白を埋めて行く。
        curpos_t xwrite = p1;
        for (auto const& range : ranges) {
          curpos_t const xL = range.first, xR = range.second;
          bool const beg_skip = _to_skip(xL);
          bool const end_skip = xL == xR ? beg_skip : _to_skip(xR);
          auto const [i1, x1] = this->_prop_lub(xL, beg_skip);
          auto const [i2, x2] = this->_prop_glb(xR, end_skip);
          contra_unused(x2);
          curpos_t x = p1 + (x1 - xL);
          for (std::size_t i = i1; i < i2; i++) {
            if (m_cells[i].is_protected()) {
              if (xwrite < x) cells.insert(cells.end(), x - xwrite, fill);
              cells.push_back(m_cells[i]);
              xwrite = x + m_cells[i].width;
            }
            x += m_cells[i].width;
          }
        }
        if (xwrite < p2) cells.insert(cells.end(), p2 - xwrite, fill);
      }

      break;
    }
  };

  for (int i = 0; i < count; i++)
    _process_segment(comp[i]);

  m_cells.swap(cells);
  m_version++;
  m_prop_i = 0;
  m_prop_x = 0;
}

void line_t::_mono_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attribute_t const& fill_attr) {
  contra_unused(flags);
  if (shift == 0) return;
  p1 = contra::clamp(p1, 0, width);
  p2 = contra::clamp(p2, 0, width);
  if (p1 >= p2) return;

  cell_t fill;
  fill.character = ascii_nul;
  fill.attribute = 0;
  fill.width = 1;
  m_cells.resize((std::size_t) width, fill);
  fill.attribute = fill_attr;

  bool protect = false;
  auto _erase_wide_left = [this, &protect] (curpos_t p1) {
    if (m_cells[p1].character.is_wide_extension()) {
      if (protect && m_cells[p1].is_protected()) return;
      curpos_t c = p1;
      if (c > 0) c--;
      while (c > 0 && m_cells[c].character.is_wide_extension()) c--;
      for (; c < p1; c++) {
        m_cells[c].character = ascii_sp;
        m_cells[c].width = 1;
      }
    }
  };

  auto _erase_wide_right = [this, &protect] (curpos_t p) {
    if (p < (curpos_t) m_cells.size() && m_cells[p].character.is_wide_extension()) {
      if (protect && m_cells[p].is_protected()) return;
      do {
        m_cells[p].character = ascii_sp;
        m_cells[p].width = 1;
        p++;
      } while (p < (curpos_t) m_cells.size() && m_cells[p].character.is_wide_extension());
    }
  };

  if (std::abs(shift) >= p2 - p1) {
    protect = flags & line_shift_flags::erm;
    _erase_wide_left(p1);
    _erase_wide_right(p2);
    if (protect) {
      while (p1 < p2) {
        if (!m_cells[p1].is_protected())
          m_cells[p1] = fill;
        p1++;
      }
    } else
      while (p1 < p2) m_cells[p1++] = fill;
  } else if (shift > 0) {
    _erase_wide_left(p1);
    _erase_wide_right(p1);
    _erase_wide_left(p2 - shift);
    _erase_wide_right(p2);
    auto it0 = m_cells.begin();
    std::move_backward(it0 + p1, it0 + (p2 - shift), it0 + p2);
    while (shift--) m_cells[p1++] = fill;
  } else {
    _erase_wide_left(p1);
    _erase_wide_right(p1 - shift);
    _erase_wide_left(p2);
    _erase_wide_right(p2);
    auto it0 = m_cells.begin();
    std::move(it0 + (p1 - shift), it0 + p2, it0 + p1);
    while (shift++) m_cells[--p2] = fill;
  }

  m_version++;
}

void line_t::_bdsm_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attribute_t const& fill_attr) {
  // m_prop_enabled && !get_mode(mode_dcsm) の時
  mwg_assert(!(flags & line_shift_flags::dcsm));

  if (shift == 0) return;
  p1 = contra::clamp(p1, 0, width);
  p2 = contra::clamp(p2, 0, width);
  if (p1 >= p2) return;

  line_segment_t segs[4];
  int iseg = 0;

  if (std::abs(shift) >= p2 - p1 && (flags & line_shift_flags::erm) && has_protected_cells()) {
    // erase unprotected cells

    // protected な cell が端にかかっている時にはそれを消去領域に含む様に拡張
    bool line_r2l = flags & line_shift_flags::r2l;
    if (p1 > 0) {
      curpos_t const x1 = convert_position(false, p1, -1, width, line_r2l);
      auto [i, x] = _prop_glb(x1, false);
      if (x < x1 && i < m_cells.size() && m_cells[i].is_protected()) {
        curpos_t const xL = x, xR = x + m_cells[i].width;
        if (x1 - xL == xR - x1) {
          p1 -= x1 - xL;
        } else {
          curpos_t const x1R = convert_position(false, p1 + 1, +1, width, line_r2l);
          p1 -= x1 < x1R ? x1 - xL : xR - x1;
        }
        // assert: ちゃんと補正できているだろうか
        mwg_assert(([this, p1, width, line_r2l] () {
              curpos_t const x1 = to_data_position(p1, width, line_r2l);
              return _prop_glb(x1, false).second == x1; })());
      }
    }
    if (p2 < width) {
      curpos_t const x2 = convert_position(false, p2, -1, width, line_r2l);
      auto [i, x] = _prop_glb(x2, false);
      if (x < x2 && i < m_cells.size() && m_cells[i].is_protected()) {
        curpos_t const xL = x, xR = x + m_cells[i].width;
        if (xR - x2 == x2 - xL) {
          p2 += xR - x2;
        } else {
          curpos_t const x2R = convert_position(false, p2 + 1, +1, width, line_r2l);
          p2 += x2 < x2R ? xR - x2 : x2 - xL;
        }
        // assert: ちゃんと補正できているだろうか
        mwg_assert(([this, p2, width, line_r2l] () {
              curpos_t const x2 = to_data_position(p2, width, line_r2l);
              return _prop_glb(x2, false).second == x2; })());
      }
    }

    if (p1 > 0)
      segs[iseg++] = line_segment_t({0, p1, line_segment_slice});
    segs[iseg++] = line_segment_t({p1, p2, line_segment_erase_unprotected});
    if (p2 < width)
      segs[iseg++] = line_segment_t({p2, width, line_segment_slice});
  } else {
    if (p1 > 0)
      segs[iseg++] = line_segment_t({0, p1, line_segment_slice});
    if (std::abs(shift) >= p2 - p1) {
      segs[iseg++] = line_segment_t({p1, p2, line_segment_erase});
    } else if (shift > 0) {
      segs[iseg++] = line_segment_t({0, shift, line_segment_erase});
      segs[iseg++] = line_segment_t({p1, p2 - shift, line_segment_slice});
    } else {
      shift = -shift;
      segs[iseg++] = line_segment_t({p1 + shift, p2, line_segment_slice});
      segs[iseg++] = line_segment_t({0, shift, line_segment_erase});
    }
    if (p2 < width)
      segs[iseg++] = line_segment_t({p2, width, line_segment_slice});
  }
  _prop_compose_segments(segs, iseg, width, fill_attr, flags & line_shift_flags::r2l, false);
}

void line_t::_prop_shift_cells(curpos_t p1, curpos_t p2, curpos_t shift, line_shift_flags flags, curpos_t width, attribute_t const& fill_attr) {
  if (shift == 0) return;
  p1 = contra::clamp(p1, 0, width);
  p2 = contra::clamp(p2, 0, width);
  if (p1 >= p2) return;

  cell_t fill;
  fill.character = ascii_nul;
  fill.attribute = 0;
  fill.width = 1;

  // 行の長さの調整
  {
    std::size_t i1;
    curpos_t x1;
    std::tie(i1, x1) = _prop_glb(width, false);
    m_cells.resize((std::size_t) (i1 + width - x1), fill);
    if (x1 < width && m_cells[i1].width > 1) {
      int const w = m_cells[i1].width;
      m_cells[i1].character = ascii_sp;
      m_cells[i1].width = 1;
      for (int i = 1; i < w; i++)
        m_cells[i1 + i] = m_cells[i1];
    }
  }
  fill.attribute = fill_attr;

  std::size_t i1, i2;
  curpos_t w1, w2;
  attribute_t attr1, attr2;
  {
    std::tie(i1, w1) = _prop_glb(p1, flags & line_shift_flags::left_inclusive);
    std::tie(i2, w2) = _prop_lub(p2, flags & line_shift_flags::right_inclusive);
    if ((w1 = std::max(0, p1 - w1)))
      attr1 = m_cells[i1].attribute;
    if ((w2 = std::max(0, w2 - p2)))
      attr1 = m_cells[i2 - 1].attribute;
  }

  std::size_t iL = 0, iR = 0;
  curpos_t wL = 0, wR = 0;
  attribute_t attrL, attrR;
  curpos_t wlfill = 0, wrfill = 0;
  bool flag_erase_unprotected = false;
  if (std::abs(shift) >= p2 - p1) {
    if ((flags & line_shift_flags::erm) && has_protected_cells()) {
      if (w1 && m_cells[i1].is_protected()) {
        p1 += m_cells[i1].width - w1;
        w1 = 0;
        i1++;
      }
      if (w2 && m_cells[i2 - 1].is_protected()) {
        p2 -= m_cells[i2].width - w2;
        w2 = 0;
        i2--;
      }
      if (p1 >= p2) return;

      curpos_t protected_count = 0;
      curpos_t total_protected_width = 0;
      for (curpos_t i = p1; i < p2; i++) {
        if (m_cells[i].is_protected()) {
          protected_count++;
          total_protected_width += m_cells[i].width;
        }
      }

      curpos_t const margin = p2 - p1 - total_protected_width;
      wlfill = protected_count + margin;
      flag_erase_unprotected = protected_count > 0;
    } else
      wlfill = p2 - p1;

  } else if (shift > 0) {
    wlfill = shift;
    curpos_t pL = p1, pR = p2 - shift;
    std::tie(iL, wL) = _prop_lub(pL, !(flags & line_shift_flags::left_inclusive));
    std::tie(iR, wR) = _prop_glb(pR, false);
    if ((wL = std::max(0, wL - pL)))
      attrL = m_cells[iL - 1].attribute;
    if ((wR = std::max(0, pR - wR)))
      attrR = m_cells[iR].attribute;
  } else {
    wrfill = -shift;
    curpos_t pL = p1 - shift, pR = p2;
    std::tie(iL, wL) = _prop_lub(pL, false);
    std::tie(iR, wR) = _prop_glb(pR, !(flags & line_shift_flags::right_inclusive));
    if ((wL = std::max(0, wL - pL)))
      attrL = m_cells[iL - 1].attribute;
    if ((wR = std::max(0, pR - wR)))
      attrR = m_cells[iR].attribute;
  }

  // 範囲の確保
  std::size_t const nwrite = w1 + wlfill + wL + (iR - iL) + wR + wrfill + w2;
  if (nwrite > i2 - i1)
    m_cells.insert(m_cells.begin() + i2, nwrite - (i2 - i1), fill);

  std::size_t i = i1;
  if (flag_erase_unprotected) {
    mwg_assert(nwrite == (std::size_t) wlfill); // wlfill 以外は 0 の筈

    auto _erase_unprotected = [&] () {
      i += wlfill;
      fill.character = ascii_nul;
      fill.attribute = fill_attr;
      auto src = std::remove_if(m_cells.begin() + i1, m_cells.begin() + i2,
        [&] (auto const& cell) { return !cell.is_protected() && cell.width == 0; });
      auto dst = m_cells.begin() + i;
      while (src-- != m_cells.begin()) {
        if (src->is_protected()) {
          if (--dst != src)
            *dst = std::move(*src);
        } else {
          std::size_t w = src->width;
          while (w--) *--dst = fill;
        }
      }
    };

    _erase_unprotected();
  } else {
    // 移動
    if (iL < iR) {
      std::size_t const iL_new = i1 + w1 + wlfill + wL;
      std::size_t const iR_new = iR + iL_new - iL;
      if (iL_new < iL)
        std::move(m_cells.begin() + iL, m_cells.begin() + iR, m_cells.begin() + iL_new);
      else if (iL_new > iL)
        std::move_backward(m_cells.begin() + iL, m_cells.begin() + iR, m_cells.begin() + iR_new);
    }

    // 余白の書き込み
    auto _write_space = [&] (curpos_t w, attribute_t const& attr) {
      if (w <= 0) return;
      fill.character = ascii_sp;
      fill.attribute = attr;
      do m_cells[i++] = fill; while (--w);
    };
    auto _write_fill = [&] (curpos_t w) {
      if (w <= 0) return;
      fill.character = ascii_nul;
      fill.attribute = fill_attr;
      do m_cells[i++] = fill; while (--w);
    };

    _write_space(w1, attr1);
    _write_fill(wlfill);
    _write_space(wL, attrL);
    i += iR - iL;
    _write_space(wR, attrR);
    _write_fill(wrfill);
    _write_space(w2, attr2);
  }

  // 余分な範囲の削除
  if (i < i2)
    m_cells.erase(m_cells.begin() + i, m_cells.begin() + i2);

  m_version++;
  m_prop_i = 0;
  m_prop_x = 0;
}
