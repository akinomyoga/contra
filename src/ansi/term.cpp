#include "term.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <mwg/except.h>
#include "../sequence.hpp"

#define contra_ansi_term_abuild_gc_threshold 0x100000

namespace contra {
namespace ansi {

  typedef bool control_function_t(term_t& term, csi_parameters& params);

  static void do_hvp_impl(term_t& term, curpos_t x, curpos_t y);
  static void do_vertical_scroll(term_t& term, curpos_t shift, curpos_t tmargin, curpos_t bmargin, curpos_t lmargin, curpos_t rmargin, bool dcsm, bool transfer);
  static void do_vertical_scroll(term_t& term, curpos_t shift, bool dcsm);

  void do_ed(term_t& term, csi_param_t param);

  //---------------------------------------------------------------------------
  // Modes

  void tstate_t::initialize_mode() {
    std::fill(std::begin(m_mode_flags), std::end(m_mode_flags), 0);
#include "../../out/gen/term.mode_init.hpp"
  }

  struct mode_dictionary_t {
    std::unordered_map<std::uint32_t, mode_t> data_ansi;
    std::unordered_map<std::uint32_t, mode_t> data_dec;

  public:
    mode_dictionary_t() {
#include "../../out/gen/term.mode_register.hpp"
    }

    void set_ansi_mode(tstate_t& s, csi_param_t param, bool value) {
      auto const it = data_ansi.find(param);
      if (it != data_ansi.end())
        s.sm_mode(it->second, value);
      else
        std::fprintf(stderr, "unrecognized ANSI mode %u\n", (unsigned) param);
    }
    void set_dec_mode(tstate_t& s, csi_param_t param, bool value) {
      auto const it = data_dec.find(param);
      if (it != data_dec.end())
        s.sm_mode(it->second, value);
      else
        std::fprintf(stderr, "unrecognized DEC mode %u\n", (unsigned) param);
    }
    int rqm_ansi_mode(tstate_t& s, csi_param_t param) {
      auto const it = data_ansi.find(param);
      return it == data_ansi.end() ? 0 : s.rqm_mode(it->second);
    }
    int rqm_dec_mode(tstate_t& s, csi_param_t param) {
      auto const it = data_dec.find(param);
      return it == data_dec.end() ? 0 : s.rqm_mode(it->second);
    }
  };
  static mode_dictionary_t mode_dictionary;

  // CSI h: SM
  bool do_sm(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_ansi_mode(s, value, true);
    return true;
  }
  // CSI l: RM
  bool do_rm(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_ansi_mode(s, value, false);
    return true;
  }
  // CSI ? h: DECSET
  bool do_decset(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_dec_mode(s, value, true);
    return true;
  }
  // CSI ? l: DECRST
  bool do_decrst(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_dec_mode(s, value, false);
    return true;
  }

  static bool do_decrqm_impl(term_t& term, csi_parameters& params, bool decmode) {
    tstate_t& s = term.state();
    csi_param_t value;
    params.read_param(value, 0);

    int result;
    if (decmode)
      result = mode_dictionary.rqm_dec_mode(s, value);
    else
      result = mode_dictionary.rqm_dec_mode(s, value);

    term.input_c1(ascii_csi);
    if (decmode) term.input_byte(ascii_question);
    term.input_unsigned(value);
    term.input_byte(ascii_semicolon);
    term.input_unsigned(result);
    term.input_byte(ascii_dollar);
    term.input_byte(ascii_y);

    return true;
  }
  // CSI $ p: DECRQM (ANSI modes)
  bool do_decrqm_ansi(term_t& term, csi_parameters& params) {
    return do_decrqm_impl(term, params, false);
  }
  // CSI ? $ p: DECRQM (DEC modes)
  bool do_decrqm_dec(term_t& term, csi_parameters& params) {
    return do_decrqm_impl(term, params, true);
  }

  // ESC =: DECKPAM
  void do_deckpam(term_t& term) {
    term.state().set_mode(mode_decnkm, true);
  }
  // ESC >: DECKPNM
  void do_deckpnm(term_t& term) {
    term.state().set_mode(mode_decnkm, false);
  }

  // Mode ?7: DECAWM
  void do_sm_decawm(term_t& term, bool value) {
    tstate_t& s = term.state();
    if (s.get_mode(mode_decawm_) == value) return;
    s.set_mode(mode_decawm_, value);

    // カーソルが行末にいる時、位置を修正する。
    if (!value)
      term.board().cur.adjust_xenl();
  }
  int do_rqm_decawm(term_t& term) {
    return term.state().get_mode(mode_decawm_) ? 1 : 2;
  }

  // Mode ?3: DECCOLM
  void do_sm_deccolm(term_t& term, bool value) {
    tstate_t& s = term.state();
    if (s.get_mode(mode_Xterm132cols)) {
      board_t& b = term.board();
      b.reset_size(value ? 132 : 80, b.height());
      if (!s.get_mode(mode_decncsm)) b.clear_screen();
      s.clear_margin();
      s.set_mode(mode_declrmm, false);
      b.cur.set(0, 0);
    }
  }
  int do_rqm_deccolm(term_t& term) {
    return term.board().width() == 132 ? 1 : 2;
  }

  // CSI $ |: DECSCPP
  bool do_decscpp(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    if (!s.cfg_decscpp_enabled) return true;

    csi_param_t param;
    params.read_param(param, 0);
    if (param == 0) param = 80;
    curpos_t cols = contra::clamp<curpos_t>(param, s.cfg_decscpp_min, s.cfg_decscpp_max);

    board_t& b = term.board();
    b.reset_size(cols, b.height());

    return true;
  }

  //---------------------------------------------------------------------------
  // basic control functions

  void do_cr(term_t& term) {
    board_t& b = term.board();
    tstate_t& s = term.state();
    line_t& line = b.line();
    term.initialize_line(line);

    curpos_t x;
    if (s.get_mode(mode_simd)) {
      x = term.implicit_sll(line);
    } else {
      x = term.implicit_slh(line);
    }

    if (!s.dcsm())
      x = b.to_data_position(b.cur.y(), x);

    b.cur.set_x(x);
  }

  static void do_generic_ff(term_t& term, int delta, bool toAppendNewLine, bool toAdjustXAsPresentationPosition) {
    if (!delta) return;

    board_t& b = term.board();
    b.cur.adjust_xenl();
    curpos_t x = b.cur.x();
    if (toAdjustXAsPresentationPosition)
      x = b.to_presentation_position(b.cur.y(), x);

    curpos_t const beg = term.implicit_sph();
    curpos_t const end = term.implicit_spl() + 1;
    curpos_t const y = b.cur.y();
    if (delta > 0) {
      if (y < end) {
        if (y + delta < end) {
          b.cur.set_y(b.cur.y() + delta);
        } else {
          b.cur.set_y(end - 1);
          if (toAppendNewLine) {
            delta -= b.cur.y() - y;
            do_vertical_scroll(term, -delta, true);
          }
        }
      }
    } else {
      if (y >= beg) {
        if (y + delta >= beg) {
          b.cur.set_y(b.cur.y() + delta);
        } else {
          b.cur.set_y(beg);
          if (toAppendNewLine) {
            delta += y - b.cur.y();
            do_vertical_scroll(term, -delta, true);
          }
        }
      }
    }

    if (toAdjustXAsPresentationPosition)
      x = b.to_data_position(b.cur.y(), x);
    b.cur.set_x(x);
  }

  void do_ind(term_t& term) {
    do_generic_ff(term, 1, true, !term.state().dcsm());
  }
  void do_ri(term_t& term) {
    do_generic_ff(term, -1, true, !term.state().dcsm());
  }
  void do_lf(term_t& term) {
    tstate_t& s = term.state();
    bool const toCallCR = s.get_mode(mode_lnm);
    do_generic_ff(term, 1, true, !toCallCR && !s.dcsm());
    if (toCallCR) do_cr(term);
  }
  void do_ff(term_t& term) {
    tstate_t& s = term.state();
    bool const toCallCR = s.ff_affected_by_lnm && s.get_mode(mode_lnm);

    if (s.ff_clearing_screen) {
      board_t& b = term.board();
      if (s.ff_using_page_home) {
        b.cur.adjust_xenl();
        curpos_t x = b.cur.x();
        curpos_t y = b.cur.y();
        if (!toCallCR && !s.get_mode(mode_bdsm))
          x = b.to_presentation_position(y, x);

        b.clear_screen(term.m_scroll_buffer);
        y = std::max(term.implicit_sph(), 0);

        if (!toCallCR && !s.get_mode(mode_bdsm))
          x = b.to_data_position(y, x);

        b.cur.set(x, y);
      } else
        b.clear_screen(term.m_scroll_buffer);
    } else {
      do_generic_ff(term, 1, true, !toCallCR && !s.get_mode(mode_bdsm));
    }

    if (toCallCR) do_cr(term);
  }
  void do_vt(term_t& term) {
    tstate_t& s = term.state();
    bool const toCallCR = s.vt_affected_by_lnm && s.get_mode(mode_lnm);
    do_generic_ff(term, 1, s.vt_appending_newline, !toCallCR && !s.get_mode(mode_bdsm));
    if (toCallCR) do_cr(term);
  }
  void do_nel(term_t& term) {
    // Note: 新しい行の SLH/SLL に移動したいので、
    // LF を実行した後に CR を実行する必要がある。
    do_lf(term);
    do_cr(term);
  }

  void do_bs(term_t& term) {
    // Note: mode_xenl, mode_decawm, mode_XtermReversewrap が絡んで来た時の振る舞いは適当である。
    board_t& b = term.board();
    tstate_t const& s = term.state();
    line_t const& line = b.line();
    if (s.get_mode(mode_simd)) {
      bool const cap_xenl = s.get_mode(mode_xenl);
      curpos_t sll = term.implicit_sll(line);
      curpos_t x = b.cur.x(), y = b.cur.y();
      bool xenl = false;
      if (b.cur.xenl() || (!cap_xenl && (x == sll || x == b.width() - 1))) {
        if (s.get_mode(mode_decawm) && s.get_mode(mode_XtermReversewrap) && y > 0) {
          y--;
          x = term.implicit_slh(b.line(y));

          // Note: xenl から前行に移った時は更に次の文字への移動を試みる。
          if (b.cur.xenl())
            sll = term.implicit_sll(b.line(y));
          else
            goto update;
        } else {
          // Note: 行末に居て前の行に戻らない時は何もしない。
          return;
        }
      }

      mwg_assert(x < b.width());
      if (x != sll && x != b.width() - 1) {
        x++;
      } else if (cap_xenl) {
        xenl = true;
        x++;
      }

    update:
      b.cur.set(x, y, xenl);
    } else {
      // Note: vttest によると xenl は補正するのが正しいらしい。
      //   しかし xterm で手動で動作を確認すると sll+1 に居ても補正しない様だ。
      //   一方で RLogin では手動で確認すると補正される。
      //   xterm で補正が起こる条件があるのだろうがよく分からないので、
      //   contra では RLogin と同様に常に補正を行う事にする。
      b.cur.adjust_xenl();

      curpos_t x = b.cur.x(), y = b.cur.y();
      curpos_t const slh = term.implicit_slh(line);
      if (x != slh && x > 0) {
        x--;
      } else if (s.get_mode(mode_decawm) && s.get_mode(mode_XtermReversewrap) && y > 0) {
        y--;
        x = term.implicit_sll(b.line(y));
      }
      b.cur.set(x, y);
    }
  }

  void do_ht(term_t& term) {
    board_t& b = term.board();

    // ToDo: tab stop の管理
    int const tabwidth = 8;
    curpos_t const sll = term.implicit_sll(b.line());
    curpos_t const xdst = std::min((b.cur.x() + tabwidth) / tabwidth * tabwidth, sll);
    b.cur.set_x(xdst);
  }

  void do_insert_graph(term_t& term, char32_t u) {
    // ToDo: 新しい行に移る時に line_limit, line_home を初期化する
    board_t& b = term.board();
    tstate_t& s = term.state();
    line_t& line = b.line();
    term.initialize_line(line);

    int char_width = s.c2w(u); // ToDo 文字幅
    if (b.cur.abuild.is_double_width()) char_width *= 2;
    if (char_width <= 0) {
      std::exit(1); // ToDo: control chars, etc.
    }

    bool const simd = s.get_mode(mode_simd);
    curpos_t const dir = simd ? -1 : 1;

    curpos_t slh = term.implicit_slh(line);
    curpos_t sll = term.implicit_sll(line);
    if (b.cur.x() < slh)
      slh = 0;
    else if (b.cur.x() > (b.cur.xenl() ? sll + 1 : sll))
      sll = b.width() - 1;

    // IRM: 書き込み時ではなくて先に shift して置く
    if (s.get_mode(mode_irm)) {
      if (simd)
        line.shift_cells(slh, b.cur.x() + 1, -char_width, lshift_dcsm, b.width(), term.fill_attr());
      else
        line.shift_cells(b.cur.x(), sll + 1, char_width, lshift_dcsm, b.width(), term.fill_attr());
    }

    if (simd) std::swap(slh, sll);

    // (行頭より後でかつ) 行末に文字が入らない時は折り返し
    curpos_t const x0 = b.cur.x();
    curpos_t const x1 = x0 + dir * (char_width - 1);
    if ((x1 - sll) * dir > 0 && (x0 - slh) * dir > 0) {
      if (!s.get_mode(mode_decawm)) return;
      do_nel(term);
      // Note: do_nel() 後に b.cur.x() in [slh, sll]
      //   は保証されていると思って良いので、
      //   現在位置が範囲外の時の sll の補正は不要。
      sll = simd ? term.implicit_slh(b.line()) : term.implicit_sll(b.line());
    }

    curpos_t const xL = simd ? b.cur.x() - (char_width - 1) : b.cur.x();

    cell_t cell;
    cell.character = u;
    cell.attribute = b.cur.abuild.attr();
    cell.width = char_width;
    b.line().write_cells(xL, &cell, 1, 1, dir);

    curpos_t x = b.cur.x() + dir * char_width;
    bool xenl = false;
    if ((x - sll) * dir >= 1) {
      if (s.get_mode(mode_decawm)) {
        // 行末を超えた時は折り返し
        // Note: xenl かつ x == sll + 1 ならば の位置にいる事を許容する。
        if (!simd && x == sll + 1 && s.get_mode(mode_xenl)) {
          xenl = true;
        } else {
          do_nel(term);
          return;
        }
      } else {
        x = sll;
      }
    }
    b.cur.set_x(x, xenl);
  }

  void do_insert_graphs(term_t& term, char32_t const* beg, char32_t const* end) {
    if (beg + 1 == end) return do_insert_graph(term, *beg);

    board_t& b = term.board();
    tstate_t& s = term.state();

    // initialize configuration
    bool const simd = s.get_mode(mode_simd);
    curpos_t const dir = simd ? -1 : 1;
    bool const decawm = s.get_mode(mode_decawm);
    bool const cap_xenl = s.get_mode(mode_xenl);
    int const width_multiplier = b.cur.abuild.is_double_width() ? 2 : 1;

    curpos_t x = b.cur.x();

    // current line and range
    line_t* line = &b.line();
    term.initialize_line(*line);
    curpos_t slh = term.implicit_slh(*line);
    curpos_t sll = term.implicit_sll(*line);
    if (x < slh)
      slh = 0;
    else if (x > (b.cur.xenl() ? sll + 1 : sll))
      sll = b.width() - 1;
    if (simd) std::swap(slh, sll);

    auto _next_line = [&] () {
      do_nel(term);
      x = b.cur.x();
      line = &b.line();
      term.initialize_line(*line);

      // Note: do_nel() 後に b.cur.x() in [slh, sll]
      //   は保証されていると思って良いので、
      //   現在位置が範囲外の時の sll の補正は不要。
      slh = term.implicit_slh(*line);
      sll = term.implicit_sll(*line);
      if (simd) std::swap(slh, sll);
    };

    cell_t cell;
    cell.character = ascii_nul;
    cell.attribute = b.cur.abuild.attr();
    cell.width = 1;

    std::vector<cell_t>& buffer = term.m_buffer;
    mwg_assert(buffer.empty());
    curpos_t xbeg = x;
    auto _write = [&] () {
      if (simd) {
        mwg_assert(sll <= x + 1 && x < xbeg, "slh=%d sll=%d xbeg=%d x=%d", slh, sll, xbeg, x);
        if (s.get_mode(mode_irm))
          line->shift_cells(sll, xbeg + 1, x - xbeg, lshift_dcsm, b.width(), term.fill_attr());
        std::reverse(buffer.begin(), buffer.end());
        line->write_cells(x + 1, &buffer[0], buffer.size(), 1, dir);
      } else {
        if (s.get_mode(mode_irm))
          line->shift_cells(xbeg, sll + 1, x - xbeg, lshift_dcsm, b.width(), term.fill_attr());
        line->write_cells(xbeg, &buffer[0], buffer.size(), 1, dir);
      }
      buffer.clear();
    };

    while (beg < end) {
      char32_t const u = *beg++;

      int const char_width = s.c2w(u) * width_multiplier;
      mwg_assert(char_width > 0); // ToDo: control chars, etc.

      // (行頭より後でかつ) 行末に文字が入らない時は折り返し
      curpos_t const x0 = x;
      curpos_t const x1 = x0 + dir * (char_width - 1);
      if ((x1 - sll) * dir > 0 && (x0 - slh) * dir > 0) {
        if (buffer.size()) _write();

        // Note: !decawm の時は以降の文字も全部入らないので抜ける。
        //   現在は文字幅が有限である事を要求しているので。
        //   但し、零幅の文字を考慮に入れる時にはここは修正する必要がある。
        //   もしくは零幅の文字の時には insert_graphs は使わない。
        mwg_assert(char_width > 0);
        if (!decawm) break;

        _next_line();
      }

      // 文字は取り敢えず buffer に登録する
      if (buffer.empty()) xbeg = x;
      cell.character = u;
      cell.width = char_width;
      buffer.emplace_back(cell);
      x += dir * char_width;
    }

    // Note: buffer.size() == 0 なのは何も書いていないか改行した直後で、
    //   その時にはデータを書き込む必要はないし、
    //   また x も動かないか改行した時に設定されている筈。
    if (buffer.size()) {
      _write();

      bool xenl = false;
      if ((x - sll) * dir >= 1) {
        if (decawm) {
          // 行末を超えた時は折り返し
          // Note: xenl かつ x == sll + 1 ならば の位置にいる事を許容する。
          if (!simd && x == sll + 1 && cap_xenl) {
            xenl = true;
          } else {
            do_nel(term);
            return;
          }
        } else {
          x = sll;
        }
      }
      b.cur.set_x(x, xenl);
    }
  }

  //---------------------------------------------------------------------------
  // Page and line settings

  bool do_spd(term_t& term, csi_parameters& params) {
    csi_param_t direction;
    params.read_param(direction, 0);
    csi_param_t update;
    params.read_param(update, 0);
    if (direction > 7 || update > 2) return false;

    board_t& b = term.board();

    bool const oldRToL = is_charpath_rtol(b.presentation_direction());
    b.set_presentation_direction((presentation_direction_t) direction);
    bool const newRToL = is_charpath_rtol(b.presentation_direction());

    /* update content
     *
     * Note: update == 2 の場合、見た目が変わらない様にデータを更新する。
     *   但し、縦書き・横書きの変更の場合には、
     *   横書きの各行が縦書きの各行になる様にする。
     *   また、line progression の変更も適用しない。
     *   単に、character path ltor/ttob <-> rtol/btot の間の変換だけ行う。
     *   今後この動作は変更する可能性がある。
     *
     */
    if (oldRToL != newRToL && update == 2) {
      for (curpos_t y = 0, yN = b.height(); y < yN; y++) {
        line_t& line = b.m_lines[y];
        if (!(line.lflags() & lattr_charpath_mask))
          line.reverse(b.width());
      }
    }

    // update position
    b.cur.set(update == 2 ? b.to_data_position(b.cur.y(), 0) : 0, 0);
    return true;
  }

  bool do_scp(term_t& term, csi_parameters& params) {
    csi_param_t charPath;
    params.read_param(charPath, 0);
    csi_param_t update;
    params.read_param(update, 0);
    if (charPath > 2 || update > 2) return false;

    tstate_t& s = term.state();
    board_t& b = term.board();
    line_t& line = b.line();

    bool const oldRToL = line.is_r2l(b.presentation_direction());

    // update line attributes
    term.initialize_line(line);
    line.lflags() &= ~lattr_charpath_mask;
    s.lflags &= ~lattr_charpath_mask;
    switch (charPath) {
    case 1:
      line.lflags() |= lattr_ltor;
      s.lflags |= lattr_ltor;
      break;
    case 2:
      line.lflags() |= lattr_rtol;
      s.lflags |= lattr_rtol;
      break;
    }

    bool const newRToL = line.is_r2l(b.presentation_direction());

    // update line content
    if (oldRToL != newRToL && update == 2)
      b.line().reverse(b.width());

    b.cur.set_x(update == 2 ? b.to_data_position(b.cur.y(), 0) : 0);
    return true;
  }

  bool do_simd(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    if (param > 1) return false;
    term.state().set_mode(mode_simd, param != 0);
    return true;
  }

  bool do_slh(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    line_t& line = b.line();

    csi_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const x = (curpos_t) param - 1;
      line.home() = x;
      if (line.limit() >= 0 && line.limit() < x)
        line.limit() = x;

      s.line_home = x;
      if (s.line_limit >= 0 && s.line_limit < x)
        s.line_limit = x;
    } else {
      line.home() = -1;
      s.line_home = -1;
    }

    return true;
  }

  bool do_sll(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    line_t& line = b.line();

    csi_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const x = (curpos_t) param - 1;
      line.limit() = x;
      if (line.home() >= 0 && line.home() > x)
        line.home() = x;

      s.line_limit = x;
      if (s.line_home >= 0 && s.line_home > x)
        s.line_home = x;
    } else {
      line.limit() = -1;
      s.line_limit = -1;
    }

    return true;
  }

  bool do_sph(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();

    csi_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const y = (curpos_t) param - 1;
      s.page_home = y;
      if (s.page_limit >= 0 && s.page_limit < y)
        s.page_limit = y;
    } else {
      s.page_home = -1;
    }
    return true;
  }

  bool do_spl(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();

    csi_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const y = (curpos_t) param - 1;
      s.page_limit = y;
      if (s.page_home >= 0 && s.page_home > y)
        s.page_home = y;
    } else {
      s.page_limit = -1;
    }
    return true;
  }

  bool do_decstbm(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    board_t& b = term.board();

    csi_param_t param1, param2;
    params.read_param(param1, 0);
    params.read_param(param2, 0);

    curpos_t const home = param1 == 0 ? 0 : std::min((curpos_t) param1 - 1, b.height());
    curpos_t const limit = param2 == 0 ? b.height() : std::min((curpos_t) param2, b.height());
    if (home + 2 <= limit) {
      s.dec_tmargin = home == 0 ? -1 : home;
      s.dec_bmargin = limit == b.height() ? -1 : limit;
      do_hvp_impl(term, 0, 0);
    }
    return true;
  }

  bool do_decslrm(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    if (!s.get_mode(mode_declrmm)) return true;

    csi_param_t param1, param2;
    params.read_param(param1, 0);
    params.read_param(param2, 0);

    curpos_t const _min = param1 == 0 ? 0 : std::min((curpos_t) param1 - 1, b.width());
    curpos_t const _max = param2 == 0 ? b.width() : std::min((curpos_t) param2, b.width());
    if (_min + 2 <= _max) {
      s.dec_lmargin = _min == 0 ? -1 : _min;
      s.dec_rmargin = _max == b.width() ? -1 : _max;
      do_hvp_impl(term, 0, 0);
    }
    return true;
  }

  //---------------------------------------------------------------------------
  // Strings

  bool do_sds(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;
    term.insert_marker(
      param == 1 ? marker_sds_l2r :
      param == 2 ? marker_sds_r2l :
      marker_sds_end);
    return true;
  }
  bool do_srs(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;
    term.insert_marker(
      param == 1 ? marker_srs_beg :
      marker_srs_end);
    return true;
  }

  //---------------------------------------------------------------------------
  // Cursor movement

  enum do_cux_direction {
    do_cux_prec_char = 0,
    do_cux_succ_char = 1,
    do_cux_prec_line = 2,
    do_cux_succ_line = 3,

    do_cux_shift = 2,
    do_cux_mask  = 0x3,
  };

  static constexpr std::uint32_t do_cux_vec_construct(presentation_direction_t dir, do_cux_direction value) {
    return value << do_cux_shift * dir;
  }
  static do_cux_direction do_cux_vec_select(std::uint32_t vec, presentation_direction_t value) {
    return do_cux_direction(vec >> do_cux_shift * (0 <= value && value < 8 ? value : 0) & do_cux_mask);
  }

  static void do_cux(term_t& term, csi_param_t param, do_cux_direction direction, bool isPresentation, bool check_stbm) {
    board_t& b = term.board();

    curpos_t y = b.cur.y();
    switch (direction) {
    case do_cux_prec_char:
      {
        curpos_t lmargin = 0;
        if (check_stbm) {
          if (is_charpath_rtol(b.presentation_direction()))
            lmargin = std::max((curpos_t) 0, b.width() - term.rmargin());
          else
            lmargin = term.lmargin();
        }

        curpos_t x = b.cur.x();
        if (isPresentation)
          x = b.to_presentation_position(y, x);
        x = std::max(x - (curpos_t) param, lmargin);
        if (isPresentation)
          x = b.to_data_position(y, x);
        b.cur.update_x(x);
      }
      break;
    case do_cux_succ_char:
      {
        curpos_t rmargin = term.board().width();
        if (check_stbm) {
          if (is_charpath_rtol(b.presentation_direction()))
            rmargin = std::max((curpos_t) 0, b.width() - term.lmargin());
          else
            rmargin = term.rmargin();
        }

        curpos_t x = b.cur.x();
        if (isPresentation)
          x = b.to_presentation_position(y, x);
        x = std::min(x + (curpos_t) param, rmargin - 1);
        if (isPresentation)
          x = b.to_data_position(y, x);
        b.cur.update_x(x);
      }
      break;
    case do_cux_prec_line:
      b.cur.adjust_xenl();
      b.cur.set_y(std::max(y - (curpos_t) param, check_stbm ? term.tmargin() : (curpos_t) 0));
      break;
    case do_cux_succ_line:
      b.cur.adjust_xenl();
      b.cur.set_y(std::min(y + (curpos_t) param, (check_stbm ? term.bmargin() : term.board().height()) - 1));
      break;
    }
  }
  static bool do_cux(term_t& term, csi_parameters& params, do_cux_direction direction, bool isPresentation, bool check_stbm) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }
    do_cux(term, param, direction, isPresentation, check_stbm);
    return true;
  }

  static constexpr std::uint32_t do_cux_vec_u
    = do_cux_vec_construct(presentation_direction_tblr, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_tbrl, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_btlr, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_btrl, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_lrtb, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_rltb, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_lrbt, do_cux_succ_line)
    | do_cux_vec_construct(presentation_direction_rlbt, do_cux_succ_line);
  static constexpr std::uint32_t do_cux_vec_d
    = do_cux_vec_construct(presentation_direction_btlr, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_btrl, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_tblr, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_tbrl, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_lrbt, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_rlbt, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_lrtb, do_cux_succ_line)
    | do_cux_vec_construct(presentation_direction_rltb, do_cux_succ_line);
  static constexpr std::uint32_t do_cux_vec_r
    = do_cux_vec_construct(presentation_direction_rltb, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_rlbt, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_lrtb, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_lrbt, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_tbrl, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_btrl, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_tblr, do_cux_succ_line)
    | do_cux_vec_construct(presentation_direction_btlr, do_cux_succ_line);
  static constexpr std::uint32_t do_cux_vec_l
    = do_cux_vec_construct(presentation_direction_lrtb, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_lrbt, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_rltb, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_rlbt, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_tblr, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_btlr, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_tbrl, do_cux_succ_line)
    | do_cux_vec_construct(presentation_direction_btrl, do_cux_succ_line);

  bool do_cuu(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_u, term.board().presentation_direction());
    return do_cux(term, params, dir, !term.state().get_mode(mode_bdsm), true);
  }
  bool do_cud(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_d, term.board().presentation_direction());
    return do_cux(term, params, dir, !term.state().get_mode(mode_bdsm), true);
  }
  bool do_cuf(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_r, term.board().presentation_direction());
    return do_cux(term, params, dir, !term.state().get_mode(mode_bdsm), true);
  }
  bool do_cub(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_l, term.board().presentation_direction());
    return do_cux(term, params, dir, !term.state().get_mode(mode_bdsm), true);
  }

  bool do_hpb(term_t& term, csi_parameters& params) {
    return do_cux(term, params, do_cux_prec_char, false, term.state().get_mode(mode_decom));
  }
  bool do_hpr(term_t& term, csi_parameters& params) {
    return do_cux(term, params, do_cux_succ_char, false, term.state().get_mode(mode_decom));
  }
  bool do_vpb(term_t& term, csi_parameters& params) {
    return do_cux(term, params, do_cux_prec_line, false, term.state().get_mode(mode_decom));
  }
  bool do_vpr(term_t& term, csi_parameters& params) {
    return do_cux(term, params, do_cux_succ_line, false, term.state().get_mode(mode_decom));
  }

  // RLogin によると ADM-3 由来の動作として上下左右に動く。
  void do_adm3_fs(term_t& term) { do_cux(term, 1, do_cux_succ_char, false, false); }
  void do_adm3_gs(term_t& term) { do_cux(term, 1, do_cux_prec_char, false, false); }
  void do_adm3_rs(term_t& term) { do_cux(term, 1, do_cux_prec_char, false, false); }
  void do_adm3_us(term_t& term) { do_cux(term, 1, do_cux_succ_char, false, false); }

  static bool do_cup(term_t& term, curpos_t x, curpos_t y) {
    board_t& b = term.board();
    tstate_t const& s = term.state();
    if (!s.get_mode(mode_bdsm))
      x = b.to_data_position(y, x);
    b.cur.set(x, y);
    return true;
  }

  bool do_cnl(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0 && s.get_mode(mode_zdm)) param = 1;

    board_t& b = term.board();
    curpos_t const y = std::min(b.cur.y() + (curpos_t) param, term.bmargin() - 1);
    return do_cup(term, 0, y);
  }

  bool do_cpl(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0 && s.get_mode(mode_zdm)) param = 1;

    board_t& b = term.board();
    curpos_t const y = std::max(b.cur.y() - (curpos_t) param, term.tmargin());
    return do_cup(term, 0, y);
  }

  bool do_cup(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param1, param2;
    params.read_param(param1, 1);
    params.read_param(param2, 1);
    if (s.get_mode(mode_zdm)) {
      if (param1 == 0) param1 = 1;
      if (param2 == 0) param2 = 1;
    }

    if (param1 == 0 || param2 == 0) return false;

    if (s.get_mode(mode_decom)) {
      param1 = std::min<csi_param_t>(param1 + term.tmargin(), term.bmargin());
      param2 = std::min<csi_param_t>(param2 + term.lmargin(), term.rmargin());
    } else {
      board_t& b = term.board();
      param1 = std::min<csi_param_t>(param1, b.height());
      param2 = std::min<csi_param_t>(param2, b.width());
    }

    return do_cup(term, (curpos_t) param2 - 1, (curpos_t) param1 - 1);
  }

  static void do_hvp_impl(term_t& term, curpos_t x, curpos_t y) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    if (s.get_mode(mode_decom)) {
      x = std::min(x + term.lmargin(), term.rmargin() - 1);
      y = std::min(y + term.tmargin(), term.bmargin() - 1);
    } else {
      if (x >= b.width()) x = b.width() - 1;
      if (y >= b.height()) y = b.height() - 1;
    }
    b.cur.set(x, y);
  }
  bool do_hvp(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param1, param2;
    params.read_param(param1, 1);
    params.read_param(param2, 1);
    if (s.get_mode(mode_zdm)) {
      if (param1 == 0) param1 = 1;
      if (param2 == 0) param2 = 1;
    }

    if (param1 == 0 || param2 == 0) return false;

    do_hvp_impl(term, (curpos_t) param2 - 1, (curpos_t) param1 - 1);
    return true;
  }

  bool do_cha(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    board_t& b = term.board();
    if (s.get_mode(mode_decom))
      param = std::min<csi_param_t>(param + term.lmargin(), term.rmargin());
    else
      param = std::min<csi_param_t>(param, b.width());
    return do_cup(term, param - 1, b.cur.y());
  }

  bool do_hpa(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    board_t& b = term.board();
    if (s.get_mode(mode_decom))
      param = std::min<csi_param_t>(param + term.lmargin(), term.rmargin());
    else
      param = std::min<csi_param_t>(param, b.width());
    b.cur.set_x(param - 1);
    return true;
  }

  bool do_vpa(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    board_t& b = term.board();
    if (s.get_mode(mode_decom))
      param = std::min<csi_param_t>(param + term.tmargin(), term.bmargin());
    else
      param = std::min<csi_param_t>(param, b.height());

    b.cur.adjust_xenl();
    b.cur.set_y(param - 1);
    return true;
  }

  //---------------------------------------------------------------------------
  // Scroll

  static void do_vertical_scroll(term_t& term, curpos_t shift, curpos_t tmargin, curpos_t bmargin, curpos_t lmargin, curpos_t rmargin, bool dcsm, bool transfer) {
    board_t& b = term.board();
    if (lmargin == 0 && rmargin == b.width()) {
      b.shift_lines(tmargin, bmargin, shift, term.fill_attr(), transfer ? &term.m_scroll_buffer : nullptr);
    } else if (lmargin < rmargin) {
      // DECSLRM が設定されている時のスクロール。行内容を切り貼りする。
      line_segment_t segs[3];
      int iseg = 0, iseg_transfer;
      if (0 < lmargin)
        segs[iseg++] = line_segment_t({0, lmargin, line_segment_slice});
      segs[iseg_transfer = iseg++] = line_segment_t({lmargin, rmargin, line_segment_transfer});
      if (rmargin < b.width())
        segs[iseg++] = line_segment_t({rmargin, b.width(), line_segment_slice});

      attr_t const fill_attr = term.fill_attr();
      if (shift > 0) {
        curpos_t ydst = bmargin - 1;
        curpos_t ysrc = ydst - shift;
        for (; tmargin <= ysrc; ydst--, ysrc--) {
          segs[iseg_transfer].source = &b.line(ysrc);
          segs[iseg_transfer].source_r2l = b.line_r2l(ysrc);
          b.line(ydst).compose_segments(segs, iseg, b.width(), fill_attr, b.line_r2l(ydst), dcsm);
        }
        segs[iseg_transfer].type = line_segment_erase;
        for (; tmargin <= ydst; ydst--)
          b.line(ydst).compose_segments(segs, iseg, b.width(), fill_attr, b.line_r2l(ydst), dcsm);
      } else if (shift < 0) {
        shift = -shift;
        curpos_t ydst = tmargin;
        curpos_t ysrc = tmargin + shift;
        for (; ysrc < bmargin; ydst++, ysrc++) {
          segs[iseg_transfer].source = &b.line(ysrc);
          segs[iseg_transfer].source_r2l = b.line_r2l(ysrc);
          b.line(ydst).compose_segments(segs, iseg, b.width(), fill_attr, b.line_r2l(ydst), dcsm);
        }
        segs[iseg_transfer].type = line_segment_erase;
        for (; ydst < bmargin; ydst++)
          b.line(ydst).compose_segments(segs, iseg, b.width(), fill_attr, b.line_r2l(ydst), dcsm);
      }
    }
  }
  static void do_vertical_scroll(term_t& term, curpos_t shift, bool dcsm) {
    curpos_t const tmargin = term.tmargin();
    curpos_t const bmargin = term.bmargin();
    curpos_t const lmargin = term.lmargin();
    curpos_t const rmargin = term.rmargin();
    do_vertical_scroll(term, shift, tmargin, bmargin, lmargin, rmargin, dcsm, true);
  }

  static bool do_scroll(term_t& term, csi_parameters& params, do_cux_direction direction, bool dcsm = false) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    board_t& b = term.board();
    bool const isPresentation = !s.get_mode(mode_bdsm);

    curpos_t const y = b.cur.y();
    curpos_t shift = 0;
    switch (direction) {
    case do_cux_prec_char:
      shift = -(curpos_t) param;
      goto shift_cells;
    case do_cux_succ_char:
      shift = (curpos_t) param;
      goto shift_cells;
    shift_cells:
      {
        curpos_t const p = isPresentation ? b.to_presentation_position(y, b.cur.x()) : -1;
        line_shift_flags flags = b.line_r2l() ? lshift_r2l : lshift_none;
        if (dcsm) flags |= lshift_dcsm;
        curpos_t const tmargin = term.tmargin();
        curpos_t const bmargin = term.bmargin();
        curpos_t const lmargin = term.lmargin();
        curpos_t const rmargin = term.rmargin();
        attr_t const fill_attr = term.fill_attr();
        for (curpos_t y = tmargin; y < bmargin; y++)
          b.line(y).shift_cells(lmargin, rmargin, shift, flags, b.width(), fill_attr);
        if (isPresentation)
          b.cur.update_x(b.to_data_position(y, p));
      }
      break;

    case do_cux_prec_line:
      shift = -(curpos_t) param;
      goto vertical_scroll;
    case do_cux_succ_line:
      shift = (curpos_t) param;
      goto vertical_scroll;
    vertical_scroll:
      {
        curpos_t const p = isPresentation ? b.to_presentation_position(y, b.cur.x()) : -1;
        do_vertical_scroll(term, shift, dcsm);
        if (isPresentation)
          b.cur.update_x(b.to_data_position(y, p));
      }
      break;
    }

    return true;
  }

  bool do_su(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_u, term.board().presentation_direction());
    return do_scroll(term, params, dir);
  }
  bool do_sd(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_d, term.board().presentation_direction());
    return do_scroll(term, params, dir);
  }
  bool do_sr(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_r, term.board().presentation_direction());
    return do_scroll(term, params, dir);
  }
  bool do_sl(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_l, term.board().presentation_direction());
    return do_scroll(term, params, dir);
  }

  //---------------------------------------------------------------------------
  // Cursor save

  bool do_scosc(term_t& term, csi_parameters& params) {
    (void) params;
    auto& s = term.state();
    auto& b = term.board();
    s.m_scosc_x = b.cur.x();
    s.m_scosc_y = b.cur.y();
    s.m_scosc_xenl = b.cur.xenl();
    return true;
  }

  static void do_restore_cursor(term_t& term, curpos_t x, curpos_t y, bool xenl) {
    board_t& b = term.board();
    if (y >= b.height()) y =  b.height() - 1;
    if (xenl) {
      if (x >= b.width()) {
        x = b.width();
      } else if (x != term.implicit_sll(b.line(y)) + 1) {
        x++;
        xenl = false;
      }
    } else {
      if (x >= b.width()) x = b.width() - 1;
    }
    b.cur.set(x, y, xenl);
  }

  bool do_scorc(term_t& term, csi_parameters& params) {
    (void) params;
    auto& s = term.state();
    if (s.m_scosc_x >= 0)
      do_restore_cursor(term, s.m_scosc_x, s.m_scosc_y, s.m_scosc_xenl);
    return true;
  }

  void do_decsc(term_t& term) {
    auto& s = term.state();
    auto& b = term.board();
    s.m_decsc_cur = b.cur;
    s.m_decsc_decawm = s.get_mode(mode_decawm);
    s.m_decsc_decom = s.get_mode(mode_decom);
  }

  void do_decrc(term_t& term) {
    auto& s = term.state();
    auto& b = term.board();
    if (s.m_decsc_cur.x() >= 0) {
      do_restore_cursor(term, s.m_decsc_cur.x(), s.m_decsc_cur.y(), s.m_decsc_cur.xenl());
      b.cur.abuild = s.m_decsc_cur.abuild;
      s.set_mode(mode_decawm, s.m_decsc_decawm);
      s.set_mode(mode_decom, s.m_decsc_decom);
    }
  }

  bool do_decslrm_or_scosc(term_t& term, csi_parameters& params) {
    if (params.size() == 0)
      return do_scosc(term, params);
    else
      return do_decslrm(term, params);
  }

  //---------------------------------------------------------------------------
  // Alternate Screen Buffer

  // Mode ?47: XTERM_ALTBUF
  int do_rqm_XtermAltbuf(term_t& term) {
    return term.state().rqm_mode(mode_altscr);
  }
  void do_sm_XtermAltbuf(term_t& term, bool value) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    if (value != s.get_mode(mode_altscr)) {
      s.set_mode(mode_altscr, value);
      s.altscreen.reset_size(b.width(), b.height());
      s.altscreen.cur = b.cur;
      s.altscreen.m_line_count = b.m_line_count;
      std::swap(s.altscreen, b);
    }
  }

  // Mode ?1047: XTERM_OPT_ALTBUF
  int do_rqm_XtermOptAltbuf(term_t& term) {
    return term.state().rqm_mode(mode_altscr);
  }
  void do_sm_XtermOptAltbuf(term_t& term, bool value) {
    auto& s = term.state();
    if (s.get_mode(mode_altscr) != value) {
      s.set_mode(mode_XtermAltbuf, value);
      if (value) do_ed(term, 2);
    }
  }

  // Mode ?1048: XTERM_SAVE_CURSOR
  int do_rqm_XtermSaveCursor(term_t& term) {
    return term.state().m_decsc_cur.x() >= 0 ? 1 : 2;
  }
  void do_sm_XtermSaveCursor(term_t& term, bool value) {
    if (value)
      do_decsc(term);
    else
      do_decrc(term);
  }

  // Mode ?1049: XTERM_OPT_ALTBUF_CURSOR
  int do_rqm_XtermOptAltbufCursor(term_t& term) {
    return term.state().rqm_mode(mode_altscr);
  }
  void do_sm_XtermOptAltbufCursor(term_t& term, bool value) {
    auto& s = term.state();
    if (s.get_mode(mode_altscr) != value) {
      s.set_mode(mode_XtermAltbuf, value);
      s.set_mode(mode_XtermSaveCursor, value);
      if (value) do_ed(term, 2);
    }
  }

  //---------------------------------------------------------------------------
  // SGR and other graphics

  static void do_sgr_iso8613_colors(board_t& b, csi_parameters& params, int type) {
    csi_param_t param2;
    params.read_arg(param2, true, 0);
    int colorSpace = color_space_fromsgr(param2);

    color_t color;
    switch (colorSpace) {
    case color_space_default:
    default:
      switch (type) {
      case 0: b.cur.abuild.reset_fg(); break;
      case 1: b.cur.abuild.reset_bg(); break;
      case 2: b.cur.abuild.reset_dc(); break;
      }
      break;

    case color_space_transparent:
      color = 0;
      goto set_color;

    case color_space_rgb:
    case color_space_cmy:
    case color_space_cmyk:
      {
        color = 0;

        int const ncomp = colorSpace == color_space_cmyk ? 4 : 3;
        csi_param_t comp;
        for (int i = 0; i < ncomp; i++) {
          params.read_arg(comp, true, 0);
          if (comp > 255) comp = 255;
          color |= comp << i * 8;
        }

        if (params.read_arg(comp, false, 0)) {
          // If there are more than `ncomp` colon-separated arguments,
          // switch to ISO 8613-6 compatible mode expecting sequences like
          //
          //     CSI 38:2:CS:R:G:B:dummy:tol:tolCS m
          //
          // In this mode, discard the first byte (CS = color space)
          // and append a new byte containing the last color component.
          color = color >> 8 | comp << (ncomp - 1) * 8;
        } else {
          // If there are no more than `ncomp` arguments,
          // switch to konsole/xterm compatible mode expecting sequences like
          //
          //   CSI 38;2;R;G;B m
          //
          // In this mode, do nothing here; We have already correct `color`.
        }
        goto set_color;
      }

    case color_space_indexed:
      if (params.read_arg(color, true, 0))
        goto set_color;
      else
        std::fprintf(stderr, "missing argument for SGR 38:5\n");
      break;

    set_color:
      switch (type) {
      case 0: b.cur.abuild.set_fg(color, colorSpace); break;
      case 1: b.cur.abuild.set_bg(color, colorSpace); break;
      case 2: b.cur.abuild.set_dc(color, colorSpace); break;
      }
      break;
    }
  }

  static void do_sgr_parameter(term_t& term, csi_param_t param, csi_parameters& rest) {
    board_t& b = term.board();
    if (30 <= param && param < 40) {
      if (param < 38) {
        b.cur.abuild.set_fg(param - 30, color_space_indexed);
      } else if (param == 38) {
        do_sgr_iso8613_colors(b, rest, 0);
      } else {
        b.cur.abuild.reset_fg();
      }
      return;
    } else if (40 <= param && param < 50) {
      if (param < 48) {
        b.cur.abuild.set_bg(param - 40, color_space_indexed);
      } else if (param == 48) {
        do_sgr_iso8613_colors(b, rest, 1);
      } else {
        b.cur.abuild.reset_bg();
      }
      return;
    } else if (90 <= param && param <= 97) {
      b.cur.abuild.set_fg(8 + (param - 90), color_space_indexed);
      return;
    } else if (100 <= param && param <= 107) {
      b.cur.abuild.set_bg(8 + (param - 100), color_space_indexed);
      return;
    }

    if (10 <= param && param <= 19) {
      b.cur.abuild.set_font((param - 10) << aflags_font_shift);
      return;
    }

    switch (param) {
    case 0: b.cur.abuild.clear_sgr(); break;

    case 1 : b.cur.abuild.set_weight(attr_bold_set); break;
    case 2 : b.cur.abuild.set_weight(attr_faint_set); break;
    case 22: b.cur.abuild.clear_weight(); break;

    case 3 : b.cur.abuild.set_shape(attr_italic_set); break;
    case 20: b.cur.abuild.set_shape(attr_fraktur_set); break;
    case 23: b.cur.abuild.clear_shape(); break;

    case 4:
      if (!rest.read_arg(param, false, 0)) param = 1;
      switch (param) {
      case 0: b.cur.abuild.clear_underline(); break;
      default:
      case 1: b.cur.abuild.set_underline(attr_underline_single); break;
      case 2: b.cur.abuild.set_underline(attr_underline_double); break;
      case 3: b.cur.abuild.set_underline(attr_underline_curly); break;
      case 4: b.cur.abuild.set_underline(attr_underline_dotted); break;
      case 5: b.cur.abuild.set_underline(attr_underline_dashed); break;
      }
      break;
    case 21: b.cur.abuild.set_underline(attr_underline_double); break;
    case 24: b.cur.abuild.clear_underline(); break;

    case 5 : b.cur.abuild.set_blink(attr_blink_set); break;
    case 6 : b.cur.abuild.set_blink(attr_rapid_blink_set); break;
    case 25: b.cur.abuild.clear_blink(); break;

    case 7 : b.cur.abuild.set_inverse(); break;
    case 27: b.cur.abuild.clear_inverse(); break;

    case 8 : b.cur.abuild.set_invisible(); break;
    case 28: b.cur.abuild.clear_invisible(); break;

    case 9 : b.cur.abuild.set_strike(); break;
    case 29: b.cur.abuild.clear_strike(); break;

    case 26: b.cur.abuild.set_proportional(); break;
    case 50: b.cur.abuild.clear_proportional(); break;

    case 51: b.cur.abuild.set_frame(xflags_frame_set); break;
    case 52: b.cur.abuild.set_frame(xflags_circle_set); break;
    case 54: b.cur.abuild.clear_frame(); break;

    case 53: b.cur.abuild.set_overline(); break;
    case 55: b.cur.abuild.clear_overline(); break;

    // ECMA-48:1986 SGR(60-65) 及び JIS X 0211 & ISO/IEC 6429:1992 SGR(66-69)
    case 60: b.cur.abuild.set_ideogram(xflags_ideogram_line_single_rb); break;
    case 61: b.cur.abuild.set_ideogram(xflags_ideogram_line_double_rb); break;
    case 62: b.cur.abuild.set_ideogram(xflags_ideogram_line_single_lt); break;
    case 63: b.cur.abuild.set_ideogram(xflags_ideogram_line_double_lt); break;
    case 66: b.cur.abuild.set_ideogram(xflags_ideogram_line_single_lb); break;
    case 67: b.cur.abuild.set_ideogram(xflags_ideogram_line_double_lb); break;
    case 68: b.cur.abuild.set_ideogram(xflags_ideogram_line_single_rt); break;
    case 69: b.cur.abuild.set_ideogram(xflags_ideogram_line_double_rt); break;
    case 64: b.cur.abuild.set_ideogram(xflags_ideogram_stress); break;
    case 65: b.cur.abuild.clear_ideogram(); break;

    // contra 拡張 (画面の横分割に対応する為には DECDWL の類を属性として提供する必要がある)
    case 9903: b.cur.abuild.set_decdhl(xflags_decdhl_upper_half); break;
    case 9904: b.cur.abuild.set_decdhl(xflags_decdhl_lower_half); break;
    case 9905: b.cur.abuild.set_decdhl(xflags_decdhl_single_width); break;
    case 9906: b.cur.abuild.set_decdhl(xflags_decdhl_double_width); break;

    // RLogin 拡張 (RLogin では 60-65 で利用できる。JIS X 0211 の記述と異なるが便利そうなので対応する)
    case 8460: b.cur.abuild.set_rlogin_rline(xflags_rlogin_single_rline); break;
    case 8461: b.cur.abuild.set_rlogin_rline(xflags_rlogin_double_rline); break;
    case 8462: b.cur.abuild.set_rlogin_lline(xflags_rlogin_single_lline); break;
    case 8463: b.cur.abuild.set_rlogin_lline(xflags_rlogin_double_lline); break;
    case 8464: b.cur.abuild.set_rlogin_double_strike(); break;
    case 8465: b.cur.abuild.clear_rlogin_ideogram(); break;

    // Mintty 拡張
    case 7773: case 73: b.cur.abuild.set_mintty_subsup(xflags_mintty_sup); break;
    case 7774: case 74: b.cur.abuild.set_mintty_subsup(xflags_mintty_sub); break;
    case 7775: case 75: b.cur.abuild.clear_mintty_subsup(); break;

    // Kitty 拡張
    case 7758: case 58: do_sgr_iso8613_colors(b, rest, 2); break;
    case 7759: case 59: b.cur.abuild.reset_dc(); break;

    default:
      std::fprintf(stderr, "unrecognized SGR value %d\n", param);
      break;
    }
  }

  bool do_sgr(term_t& term, csi_parameters& params) {
    if (params.size() == 0 || !term.state().get_mode(mode_grcm))
      do_sgr_parameter(term, 0, params);

    term.gc(contra_ansi_term_abuild_gc_threshold);

    csi_param_t value;
    while (params.read_param(value, 0))
      do_sgr_parameter(term, value, params);

    return true;
  }

  bool do_sco(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    if (param > 7) return false;

    term.gc(contra_ansi_term_abuild_gc_threshold);
    term.board().cur.abuild.set_sco(param << xflags_sco_shift);
    return true;
  }

  void do_plu(term_t& term) {
    term.gc(contra_ansi_term_abuild_gc_threshold);
    term.board().cur.abuild.plu();
  }

  void do_pld(term_t& term) {
    term.gc(contra_ansi_term_abuild_gc_threshold);
    term.board().cur.abuild.pld();
  }

  bool do_decsca(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;

    term.gc(contra_ansi_term_abuild_gc_threshold);
    if (param == 1)
      term.board().cur.abuild.set_decsca();
    else
      term.board().cur.abuild.clear_decsca();
    return true;
  }

  void do_spa(term_t& term) {
    term.gc(contra_ansi_term_abuild_gc_threshold);
    term.board().cur.abuild.set_spa();
  }
  void do_epa(term_t& term) {
    term.gc(contra_ansi_term_abuild_gc_threshold);
    term.board().cur.abuild.clear_spa();
  }
  void do_ssa(term_t& term) {
    term.gc(contra_ansi_term_abuild_gc_threshold);
    term.board().cur.abuild.set_ssa();
  }
  void do_esa(term_t& term) {
    term.gc(contra_ansi_term_abuild_gc_threshold);
    term.board().cur.abuild.clear_ssa();
  }

  void do_sm_decscnm(term_t& term, bool value) {
    tstate_t& s = term.state();
    if (s.get_mode(mode_decscnm_) == value) return;
    std::swap(s.m_default_fg_color, s.m_default_bg_color);
    std::swap(s.m_default_fg_space, s.m_default_bg_space);
    s.set_mode(mode_decscnm_, value);
  }
  int do_rqm_decscnm(term_t& term) {
    tstate_t& s = term.state();
    return s.get_mode(mode_decscnm_) ? 1 : 2;
  }

  //---------------------------------------------------------------------------
  // ECH, DCH, ICH

  bool do_ech(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    board_t& b = term.board();
    line_shift_flags flags = b.line_r2l() ? lshift_r2l : lshift_none;
    if (!s.get_mode(mode_erm)) flags |= lshift_erm_protect;

    curpos_t x1;
    if (s.get_mode(mode_xenl_ech)) b.cur.adjust_xenl();
    if (s.dcsm()) {
      // DCSM(DATA)
      flags |= lshift_dcsm;
      x1 = b.cur.x();
    } else {
      // DCSM(PRESENTATION)
      x1 = b.to_presentation_position(b.cur.y(), b.cur.x());
    }
    mwg_assert(x1 <= b.width());

    curpos_t const x2 = std::min(x1 + (curpos_t) param, b.width());
    b.line().shift_cells(x1, x2, x2 - x1, flags, b.width(), term.fill_attr());

    // カーソル位置
    if (!s.dcsm())
      x1 = b.to_data_position(b.cur.y(), x1);
    b.cur.update_x(x1);

    return true;
  }

  static void do_ich_impl(term_t& term, curpos_t shift) {
    board_t& b = term.board();
    tstate_t const& s = term.state();
    line_shift_flags flags = b.line_r2l() ? lshift_r2l : lshift_none;

    curpos_t x1;
    if (s.get_mode(mode_xenl_ech)) b.cur.adjust_xenl();
    if (s.dcsm()) {
      // DCSM(DATA)
      flags |= lshift_dcsm;
      x1 = b.cur.x();
    } else {
      // DCSM(PRESENTATION)
      x1 = b.to_presentation_position(b.cur.y(), b.cur.x());
    }
    mwg_assert(x1 <= b.width());

    line_t& line = b.line();
    curpos_t const slh = term.implicit_slh(line);
    curpos_t const sll = term.implicit_sll(line);

    if (!s.get_mode(mode_hem))
      line.shift_cells(x1, sll + 1, shift, flags, b.width(), term.fill_attr());
    else {
      // Note #0229: そこにある文字に関係なく x1 + 1 から挿入・削除を行う。
      line.shift_cells(slh, x1 + 1, -shift, flags, b.width(), term.fill_attr());
    }

    // カーソル位置
    if (s.dcsm()) {
      if (s.get_mode(mode_home_il)) x1 = slh;
    } else {
      x1 = s.get_mode(mode_home_il) ? slh : b.to_data_position(b.cur.y(), x1);
    }
    b.cur.update_x(x1);
  }

  bool do_ich(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    do_ich_impl(term, (curpos_t) param);
    return true;
  }

  bool do_dch(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    do_ich_impl(term, -(curpos_t) param);
    return true;
  }

  //---------------------------------------------------------------------------
  // EL, IL, DL

  static void do_el(term_t& term, line_t& line, csi_param_t param, attr_t const& fill_attr) {
    board_t& b = term.board();
    tstate_t& s = term.state();
    if (param != 0 && param != 1) {
      if (!s.get_mode(mode_erm) && line.has_protected_cells()) {
        line_shift_flags flags = lshift_erm_protect;
        if (line.is_r2l(b.presentation_direction())) flags |= lshift_r2l;
        if (s.dcsm()) flags |= lshift_dcsm;
        line.shift_cells(0, b.width(), b.width(), flags, b.width(), fill_attr);
      } else
        line.clear_content(b.width(), fill_attr);
      return;
    }

    line_shift_flags flags = 0;
    if (!s.get_mode(mode_erm)) flags |= lshift_erm_protect;
    if (line.is_r2l(b.presentation_direction())) flags |= lshift_r2l;
    if (s.dcsm()) flags |= lshift_dcsm;

    if (s.get_mode(mode_xenl_ech)) b.cur.adjust_xenl();
    curpos_t x1 = b.cur.x();
    if (!s.dcsm()) x1 = b.to_presentation_position(b.cur.y(), x1);
    mwg_assert(x1 <= b.width());

    if (param == 0)
      line.shift_cells(x1, b.width(), b.width() - x1, flags, b.width(), fill_attr);
    else
      line.shift_cells(0, x1 + 1, x1 + 1, flags, b.width(), fill_attr);

    if (!s.dcsm()) x1 = b.to_data_position(b.cur.y(), x1);
    b.cur.update_x(x1);
  }

  bool do_el(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    do_el(term, term.board().line(), param, term.fill_attr());
    return true;
  }

  void do_ed(term_t& term, csi_param_t param) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    attr_t const fill_attr = term.fill_attr();
    curpos_t y1 = 0, y2 = 0;
    if (param != 0 && param != 1) {
      y1 = 0;
      y2 = b.height();
    } else {
      do_el(term, b.line(), param, fill_attr);
      if (param == 0) {
        y1 = b.cur.y() + 1;
        y2 = b.height();
      } else {
        y1 = 0;
        y2 = b.cur.y();
      }
    }

    if (!s.get_mode(mode_erm)) {
      for (curpos_t y = y1; y < y2; y++)
        do_el(term, b.m_lines[y], 2, fill_attr);
    } else {
      for (curpos_t y = y1; y < y2; y++)
        b.m_lines[y].clear_content(b.width(), fill_attr);
    }
  }
  bool do_ed(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    do_ed(term, param);
    return true;
  }
  void do_decaln(term_t& term) {
    board_t& b = term.board();
    curpos_t const height = b.height();
    curpos_t const width = b.width();
    cell_t fill = ascii_E;
    for (curpos_t y = 0; y < height; y++) {
      line_t& line = b.line(y);
      line.clear_content();
      line.insert_cells(0, &fill, 1, width);
    }

    term.state().clear_margin();
    b.cur.set(0, 0);
  }

  static void do_il_impl(term_t& term, curpos_t shift) {
    board_t& b = term.board();
    tstate_t& s = term.state();

    // カーソル表示位置
    curpos_t p = 0;
    if (!s.get_mode(mode_home_il) && !s.dcsm())
      p = b.to_presentation_position(b.cur.y(), b.cur.x());

    // 挿入
    curpos_t const beg = term.tmargin();
    curpos_t const end = term.bmargin();
    curpos_t const lmargin = term.lmargin();
    curpos_t const rmargin = term.rmargin();
    if (beg <= b.cur.y() && b.cur.y() < end) {
      if (!s.get_mode(mode_vem)) {
        do_vertical_scroll(term, shift, b.cur.y(), end, lmargin, rmargin, s.dcsm(), false);
      } else {
        do_vertical_scroll(term, -shift, beg, b.cur.y() + 1, lmargin, rmargin, s.dcsm(), false);
      }
    }

    // カーソル位置設定
    if (s.get_mode(mode_home_il)) {
      term.initialize_line(b.line());
      b.cur.set_x(term.implicit_slh());
    } else if (!s.dcsm()) {
      curpos_t const x1 = b.to_data_position(b.cur.y(), p);
      b.cur.update_x(x1);
    }
  }

  bool do_il(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }
    do_il_impl(term, (curpos_t) param);
    return true;
  }

  bool do_dl(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }
    do_il_impl(term, -(curpos_t) param);
    return true;
  }

  //---------------------------------------------------------------------------
  // Cursor appearance

  // CSI SP q: DECSCUSR
  bool do_decscusr(term_t& term, csi_parameters& params) {
    csi_param_t spec;
    params.read_param(spec, 0);
    tstate_t& s = term.state();
    auto _set = [&s] (bool blink, int shape) {
      s.set_mode(mode_Att610Blink, blink);
      s.m_cursor_shape = shape;
    };

    switch (spec) {
    case 0: _set(true ,   0); break;
    case 1: _set(true , 100); break;
    case 2: _set(false, 100); break;
    case 3: _set(true ,   1); break;
    case 4: _set(false,   1); break;
    case 5: _set(true ,  -1); break;
    case 6: _set(false,  -1); break;

    default:
      // Cygwin (mintty) percentage of cursor height
      if (spec >= 100)
        s.m_cursor_shape = 100; // block
      else
        s.m_cursor_shape = spec;
      break;
    }
    return true;
  }

  // Mode 33: WYSTCURM (Wyse Set Cursor Mode)
  int do_rqm_wystcurm(term_t& term) {
    return !term.state().get_mode(mode_XtermCursorBlinkOps) ? 1 : 2;
  }
  void do_sm_wystcurm(term_t& term, bool value) {
    term.state().set_mode(mode_XtermCursorBlinkOps, !value);
  }

  // Mode 34: WYULCURM (Wyse Underline Cursor Mode)
  int do_rqm_wyulcurm(term_t& term) {
    return term.state().m_cursor_shape > 0 ? 1 : 2;
  }
  void do_sm_wyulcurm(term_t& term, bool value) {
    auto& s = term.state();
    if (value) {
      if (s.m_cursor_shape <= 0) s.m_cursor_shape = 1;
    } else {
      if (s.m_cursor_shape > 0) s.m_cursor_shape = 0;
    }
  }

  //---------------------------------------------------------------------------
  // Function key mode settings

  static void do_set_funckey_mode(term_t& term, std::uint32_t spec) {
    tstate_t& s = term.state();
    s.m_funckey_flags = (s.m_funckey_flags & ~funckey_mode_mask) | spec;
  }
  static int do_rqm_funckey_mode(term_t& term, std::uint32_t spec) {
    tstate_t& s = term.state();
    return (s.m_funckey_flags & funckey_mode_mask) == spec ? 1 : 2;
  }
  void do_sm_XtermTcapFkeys  (term_t& term, bool value) { do_set_funckey_mode(term, value ? (std::uint32_t) funckey_terminfo : 0); }
  void do_sm_XtermSunFkeys   (term_t& term, bool value) { do_set_funckey_mode(term, value ? (std::uint32_t) funckey_sun      : 0); }
  void do_sm_XtermHpFkeys    (term_t& term, bool value) { do_set_funckey_mode(term, value ? (std::uint32_t) funckey_hp       : 0); }
  void do_sm_XtermScoFkeys   (term_t& term, bool value) { do_set_funckey_mode(term, value ? (std::uint32_t) funckey_sco      : 0); }
  void do_sm_XtermLegacyFkeys(term_t& term, bool value) { do_set_funckey_mode(term, value ? (std::uint32_t) funckey_x11r6    : 0); }
  void do_sm_XtermVt220Fkeys (term_t& term, bool value) { do_set_funckey_mode(term, value ? (std::uint32_t) funckey_vt220    : 0); }
  void do_sm_ContraTildeFkeys(term_t& term, bool value) { do_set_funckey_mode(term, value ? (std::uint32_t) funckey_contra   : 0); }
  int do_rqm_XtermTcapFkeys  (term_t& term) { return do_rqm_funckey_mode(term, funckey_terminfo); }
  int do_rqm_XtermSunFkeys   (term_t& term) { return do_rqm_funckey_mode(term, funckey_sun     ); }
  int do_rqm_XtermHpFkeys    (term_t& term) { return do_rqm_funckey_mode(term, funckey_hp      ); }
  int do_rqm_XtermScoFkeys   (term_t& term) { return do_rqm_funckey_mode(term, funckey_sco     ); }
  int do_rqm_XtermLegacyFkeys(term_t& term) { return do_rqm_funckey_mode(term, funckey_x11r6   ); }
  int do_rqm_XtermVt220Fkeys (term_t& term) { return do_rqm_funckey_mode(term, funckey_vt220   ); }
  int do_rqm_ContraTildeFkeys(term_t& term) { return do_rqm_funckey_mode(term, funckey_contra  ); }

  bool do_XtermSetModFkeys(term_t& term, csi_parameters& params) {
    csi_param_t category, value;
    params.read_param(category, 9900);
    params.read_param(value, 2); // contra 既定値
    if (value > 3) return false;
    auto& s = term.state();
    if (category <= 5) {
      int const shift = 8 + 4 * category;
      std::uint32_t mask = (std::uint32_t) funckey_mlevel_mask << shift;
      s.m_funckey_flags = (s.m_funckey_flags & ~mask) | value << shift;
      return true;
    } else if (category == 9900) {
      s.m_funckey_flags = (s.m_funckey_flags & ~funckey_modifyKeys_mask) | value * 0x11111100;
      return true;
    }
    return false;
  }
  bool do_XtermSetModFkeys0(term_t& term, csi_parameters& params) {
    csi_param_t category;
    params.read_param(category, 9900);
    auto& s = term.state();
    if (category <= 5) {
      int const shift = 8 + 4 * category;
      std::uint32_t mask = (std::uint32_t) funckey_mlevel_mask << shift;
      s.m_funckey_flags = (s.m_funckey_flags & ~mask) | funckey_mlevel_none << shift;
      return true;
    } else if (category == 9900) {
      s.m_funckey_flags &= ~funckey_modifyKeys_mask;
      return true;
    }
    return false;
  }

  //---------------------------------------------------------------------------
  // Mouse report settings

  static void do_set_mouse_report(term_t& term, std::uint32_t spec) {
    auto& s = term.state();
    s.mouse_mode = (s.mouse_mode & ~mouse_report_mask) | spec;
  }
  static void do_set_mouse_sequence(term_t& term, std::uint32_t spec) {
    auto& s = term.state();
    s.mouse_mode = (s.mouse_mode & ~mouse_sequence_mask) | spec;
  }
  static int do_rqm_mouse_report(term_t& term, std::uint32_t spec) {
    auto& s = term.state();
    return (s.mouse_mode & mouse_report_mask) == spec ? 1 : 2;
  }
  static int do_rqm_mouse_sequence(term_t& term, std::uint32_t spec) {
    auto& s = term.state();
    return (s.mouse_mode & mouse_sequence_mask) == spec ? 1 : 2;
  }
  void do_sm_XtermX10Mouse           (term_t& term, bool value) { do_set_mouse_report(term, value ? mouse_report_down : 0); }
  void do_sm_UrxvtExtModeMouse       (term_t& term, bool value) { do_set_mouse_report(term, value ? mouse_report_UrxvtExtModeMouse : 0); }
  void do_sm_XtermVt200Mouse         (term_t& term, bool value) { do_set_mouse_report(term, value ? mouse_report_XtermVt200Mouse : 0); }
  void do_sm_XtermVt200HighlightMouse(term_t& term, bool value) { do_set_mouse_report(term, value ? mouse_report_XtermVt200HighlightMouse : 0); }
  void do_sm_XtermBtnEventMouse      (term_t& term, bool value) { do_set_mouse_report(term, value ? mouse_report_XtermBtnEventMouse : 0); }
  void do_sm_XtermFocusEventMouse    (term_t& term, bool value) { do_set_mouse_sequence(term, value ? mouse_sequence_utf8 : 0); }
  void do_sm_XtermExtModeMouse       (term_t& term, bool value) { do_set_mouse_sequence(term, value ? mouse_sequence_sgr : 0); }
  void do_sm_XtermSgrExtModeMouse    (term_t& term, bool value) { do_set_mouse_sequence(term, value ? mouse_sequence_urxvt : 0); }

  int do_rqm_XtermX10Mouse           (term_t& term) { return do_rqm_mouse_report(term, mouse_report_down); }
  int do_rqm_UrxvtExtModeMouse       (term_t& term) { return do_rqm_mouse_report(term, mouse_report_UrxvtExtModeMouse); }
  int do_rqm_XtermVt200Mouse         (term_t& term) { return do_rqm_mouse_report(term, mouse_report_XtermVt200Mouse); }
  int do_rqm_XtermVt200HighlightMouse(term_t& term) { return do_rqm_mouse_report(term, mouse_report_XtermVt200HighlightMouse); }
  int do_rqm_XtermBtnEventMouse      (term_t& term) { return do_rqm_mouse_report(term, mouse_report_XtermBtnEventMouse); }
  int do_rqm_XtermFocusEventMouse    (term_t& term) { return do_rqm_mouse_sequence(term, mouse_sequence_utf8); }
  int do_rqm_XtermExtModeMouse       (term_t& term) { return do_rqm_mouse_sequence(term, mouse_sequence_sgr); }
  int do_rqm_XtermSgrExtModeMouse    (term_t& term) { return do_rqm_mouse_sequence(term, mouse_sequence_urxvt); }

  //---------------------------------------------------------------------------
  // device attributes

  void do_s7c1t(term_t& term) {
    term.state().set_mode(mode_s7c1t, true);
  }
  void do_s8c1t(term_t& term) {
    term.state().set_mode(mode_s7c1t, false);
  }
  bool do_decscl(term_t& term, csi_parameters& params) {
    csi_param_t param, s7c1t;
    params.read_param(param, 0);
    params.read_param(s7c1t, 0);
    if (param == 0) param = 65;

    term.state().m_decscl = param;
    term.state().set_mode(mode_s7c1t, param == 1);
    return true;
  }
  bool do_da1(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    if (param != 0) return false;

    term.input_c1(ascii_csi);
    term.input_byte(ascii_question);
    term.input_unsigned(64);
    term.input_byte(ascii_semicolon);
    term.input_unsigned(21);
    term.input_byte(ascii_c);
    term.input_flush();
    return true;
  }
  bool do_da2(term_t& term, csi_parameters& params) {
    csi_param_t param;
    params.read_param(param, 0);
    if (param != 0) return false;

    term.input_c1(ascii_csi);
    term.input_byte(ascii_greater);
    term.input_unsigned(99);
    term.input_byte(ascii_semicolon);
    term.input_unsigned(0);
    term.input_byte(ascii_c);
    term.input_flush();
    return true;
  }
  bool do_decrqss(term_t& term, char32_t const* param, std::size_t len) {
    if (len == 2) {
      auto _start = [&term] {
        term.input_c1(ascii_dcs);
        term.input_byte(ascii_0);
        term.input_byte(ascii_dollar);
        term.input_byte(ascii_r);
      };
      auto _end = [&term] {
        term.input_c1(ascii_st);
      };

      if (param[0] == ascii_double_quote) {
        switch (param[1]) {
        case ascii_p:
          // DECSCL
          _start();
          term.input_unsigned(term.state().m_decscl);
          if (term.state().get_mode(mode_s7c1t)) {
            term.input_byte(ascii_semicolon);
            term.input_byte(ascii_1);
          }
          term.input_byte(ascii_p);
          _end();
          term.input_flush();
          return true;
        case ascii_q:
          // DECSCA
          _start();
          if (term.board().cur.abuild.is_decsca_protected())
            term.input_byte(ascii_1);
          term.input_byte(ascii_double_quote);
          term.input_byte(ascii_q);
          _end();
          term.input_flush();
          return true;
        }
      }
    }

    return false;
  }

  //---------------------------------------------------------------------------
  // dispatch

  constexpr std::uint32_t compose_bytes(byte major, byte minor) {
    return minor << 8 | major;
  }

  int tstate_t::rqm_mode_with_accessor(mode_t modeSpec) const {
    switch (mode_index(modeSpec)) {
#include "../../out/gen/term.mode_rqm.hpp"
    default: return 0;
    }
  }

  void tstate_t::set_mode_with_accessor(mode_t modeSpec, bool value) {
    switch (mode_index(modeSpec)) {
#include "../../out/gen/term.mode_set.hpp"
    default: ;
    }
  }

  struct control_function_dictionary {
    std::unordered_map<std::uint32_t, control_function_t*> data0;
    control_function_t* data1[63];
    control_function_t* data_0Ft[63];
    control_function_t* data_nFp[15 * 15];
    control_function_t* data_PFt[4 * 63];

    control_function_t* get(byte F) const {
      int const index = (int) F - ascii_at;
      return 0 <= index && index < (int) std::size(data1) ? data1[index] : nullptr;
    }
    void register_cfunc(control_function_t* fp, byte F) {
      int const index = (int) F - ascii_at;
      mwg_assert(0 <= index && index < (int) std::size(data1), "final byte out of range");
      mwg_assert(data1[index] == nullptr, "another function is already registered");
      data1[index] = fp;
    }

    control_function_t* get(byte I, byte F) const {
      auto const it = data0.find(compose_bytes(I, F));
      return it != data0.end() ? it->second : nullptr;
    }
    control_function_t* get_IF(byte I, byte F) const {
      if (I == ascii_sp) {
        return data_0Ft[(int) F - ascii_at];
      } else if (ascii_p <= F) {
        int const zone = (int) I - ascii_exclamation;
        int const point = (int) F - ascii_p;
        return data_nFp[zone * 15 + point];
      } else
        return get(I, F);
    }
    control_function_t* get_PF(byte P, byte F) const {
      int const zone = (int) P - ascii_less;
      int const point = (int) F - ascii_at;
      return data_PFt[zone * 63 + point];
    }
    void register_cfunc(control_function_t* fp, byte I, byte F) {
      if ((I & 0xF0) == ascii_sp) {
        if (I == ascii_sp) {
          data_0Ft[(int) F - ascii_at] = fp;
        } else if (ascii_p <= F) {
          int const zone = (int) I - ascii_exclamation;
          int const point = (int) F - ascii_p;
          data_nFp[zone * 15 + point] = fp;
        } else
          data0[compose_bytes(I, F)] = fp;
      } else {
        int const zone = (int) I - ascii_less;
        int const point = (int) F - ascii_at;
        data_PFt[zone * 63 + point] = fp;
      }
    }

    control_function_t* get(byte I1, byte I2, byte F) const {
      auto const it = data0.find(I1 << 16 | I2 << 8 | F);
      return it != data0.end() ? it->second : nullptr;
    }
    void register_cfunc(control_function_t* fp, byte I1, byte I2, byte F) {
      data0[I1 << 16 | I2 << 8 | F] = fp;
    }

    control_function_dictionary() {
      std::fill(std::begin(data1), std::end(data1), nullptr);
      std::fill(std::begin(data_0Ft), std::end(data_0Ft), nullptr);
      std::fill(std::begin(data_nFp), std::end(data_nFp), nullptr);
      std::fill(std::begin(data_PFt), std::end(data_PFt), nullptr);

      register_cfunc(&do_simd, ascii_circumflex);

      register_cfunc(&do_ich , ascii_at);
      register_cfunc(&do_cuu , ascii_A);
      register_cfunc(&do_cud , ascii_B);
      register_cfunc(&do_cuf , ascii_C);
      register_cfunc(&do_cub , ascii_D);
      register_cfunc(&do_cnl , ascii_E);
      register_cfunc(&do_cpl , ascii_F);
      register_cfunc(&do_cha , ascii_G);
      register_cfunc(&do_cup , ascii_H);
      register_cfunc(&do_ed  , ascii_J);
      register_cfunc(&do_el  , ascii_K);
      register_cfunc(&do_il  , ascii_L);
      register_cfunc(&do_dl  , ascii_M);
      register_cfunc(&do_dch , ascii_P);
      register_cfunc(&do_su  , ascii_S);
      register_cfunc(&do_sd  , ascii_T);
      register_cfunc(&do_ech , ascii_X);
      register_cfunc(&do_srs , ascii_left_bracket);
      register_cfunc(&do_sds , ascii_right_bracket);

      register_cfunc(&do_hpa , ascii_back_quote);
      register_cfunc(&do_hpr , ascii_a);
      register_cfunc(&do_da1 , ascii_c);
      register_cfunc(&do_vpa , ascii_d);
      register_cfunc(&do_vpr , ascii_e);
      register_cfunc(&do_hvp , ascii_f);
      register_cfunc(&do_sm  , ascii_h);
      register_cfunc(&do_hpb , ascii_j);
      register_cfunc(&do_vpb , ascii_k);
      register_cfunc(&do_rm  , ascii_l);
      register_cfunc(&do_sgr , ascii_m);

      register_cfunc(&do_sl  , ascii_sp, ascii_at);
      register_cfunc(&do_sr  , ascii_sp, ascii_A);
      register_cfunc(&do_spd , ascii_sp, ascii_S);
      register_cfunc(&do_slh , ascii_sp, ascii_U);
      register_cfunc(&do_sll , ascii_sp, ascii_V);
      register_cfunc(&do_sco , ascii_sp, ascii_e);
      register_cfunc(&do_sph , ascii_sp, ascii_i);
      register_cfunc(&do_spl , ascii_sp, ascii_j);
      register_cfunc(&do_scp , ascii_sp, ascii_k);

      register_cfunc(&do_decstbm         , ascii_r);
      register_cfunc(&do_decslrm_or_scosc, ascii_s);
      register_cfunc(&do_scorc   , ascii_u);
      register_cfunc(&do_decscusr, ascii_sp, ascii_q);
      register_cfunc(&do_decscl  , ascii_double_quote, ascii_p);
      register_cfunc(&do_decsca  , ascii_double_quote, ascii_q);
      register_cfunc(&do_decscpp , ascii_dollar, ascii_vertical_bar);
      register_cfunc(&do_decrqm_ansi, ascii_dollar, ascii_p);

      // CSI P Ft
      register_cfunc(&do_decset, ascii_question, ascii_h);
      register_cfunc(&do_decrst, ascii_question, ascii_l);
      register_cfunc(&do_da2              , ascii_greater, ascii_c);
      register_cfunc(&do_XtermSetModFkeys , ascii_greater, ascii_m);
      register_cfunc(&do_XtermSetModFkeys0, ascii_greater, ascii_n);

      // CSI P I Ft
      register_cfunc(&do_decrqm_dec, ascii_question, ascii_dollar, ascii_p);
    }

  };

  static control_function_dictionary cfunc_dict;

  void term_t::print_unrecognized_sequence(sequence const& seq) {
    return; // 今は表示しない (後でロギングの枠組みを整理する)

    const char* name = "sequence";
    switch (seq.type()) {
    case ascii_csi:
      name = "control sequence";
      break;
    case ascii_esc:
      name = "escape sequence";
      break;
    case ascii_sos:
      name = "character string";
      break;
    case ascii_pm:
    case ascii_apc:
    case ascii_dcs:
    case ascii_osc:
      name = "command string";
      break;
    }

    std::fprintf(stderr, "unrecognized %s: ", name);
    seq.print(stderr, 100);
    std::fputc('\n', stderr);
  }

  void term_t::process_control_sequence(sequence const& seq) {
    // check cursor state
#ifndef NDEBUG
    board_t& b = this->board();
    mwg_assert(b.cur.is_sane(b.width()));
#endif

    auto& params = this->w_csiparams;
    params.initialize(seq);
    if (!params) {
      switch (params.result_code()) {
      default:
      case csi_parameters::parse_invalid:
        std::fprintf(stderr, "invalid value of CSI parameter values.\n");
        break;
      case csi_parameters::parse_overflow:
        std::fprintf(stderr, "a CSI parameter value is too large.\n");
        break;
      }
      print_unrecognized_sequence(seq);
      return;
    }

    bool result = false;

    switch (params.private_prefix_count()) {
    case 0:
      switch (seq.intermediate_size()) {
      case 0: // CSI Ft
        if (seq.final() == ascii_m)
          result = do_sgr(*this, params);
        else if (control_function_t* const f = cfunc_dict.get(seq.final()))
          result = f(*this, params);
        break;

      case 1: // CSI I Ft
        mwg_assert(seq.intermediate()[0] <= 0xFF);
        if (control_function_t* const f = cfunc_dict.get_IF((byte) seq.intermediate()[0], seq.final()))
          result = f(*this, params);
        break;

      case 2: // CSI I I Ft
        {
          byte const I1 = seq.intermediate()[0];
          byte const I2 = seq.intermediate()[1];
          byte const F = seq.final();
          if (control_function_t* const f = cfunc_dict.get(I1, I2, F))
            result = f(*this, params);
        }
        break;
      }
      break;

    case 1: // Private parameter byte
      switch (seq.intermediate_size()) {
      case 0:
        if (control_function_t* const f = cfunc_dict.get_PF((byte) seq.parameter()[0], seq.final()))
          result = f(*this, params);
        break;
      case 1:
        {
          byte const P = seq.parameter()[0];
          byte const I = seq.intermediate()[0];
          byte const F = seq.final();
          if (control_function_t* const f = cfunc_dict.get(P, I, F))
            result = f(*this, params);
          break;
        }
      }
      break;
    }

#ifndef NDEBUG
    mwg_assert(b.cur.is_sane(b.width()),
      "cur: {x=%d, xenl=%d, width=%d} after CSI %c %c",
      b.cur.x(), b.cur.xenl(), b.width(), seq.parameter()[0], seq.final());
#endif

    if (!result)
      print_unrecognized_sequence(seq);
  }

  void term_t::process_control_character(char32_t uchar) {
    // 頻出制御文字
    if (uchar == ascii_lf) {
      do_lf(*this);
    } else if (uchar == ascii_cr) {
      do_cr(*this);
    } else {
      switch (uchar) {
      case ascii_bel: do_bel(); break;
      case ascii_bs : do_bs(*this);  break;
      case ascii_ht : do_ht(*this);  break;
      //case ascii_lf : do_lf(*this);  break;
      case ascii_ff : do_ff(*this);  break;
      case ascii_vt : do_vt(*this);  break;
      //case ascii_cr : do_cr(*this);  break;

      case ascii_fs: do_adm3_fs(*this); break;
      case ascii_gs: do_adm3_gs(*this); break;
      case ascii_rs: do_adm3_rs(*this); break;
      case ascii_us: do_adm3_us(*this); break;

      case ascii_ind: do_ind(*this); break;
      case ascii_nel: do_nel(*this); break;
      case ascii_ri : do_ri(*this) ; break;

      case ascii_pld: do_pld(*this); break;
      case ascii_plu: do_plu(*this); break;
      case ascii_spa: do_spa(*this); break;
      case ascii_epa: do_epa(*this); break;
      case ascii_ssa: do_ssa(*this); break;
      case ascii_esa: do_esa(*this); break;
      }
    }

#ifndef NDEBUG
    board_t& b = board();
    mwg_assert(b.cur.is_sane(b.width()),
      "cur: {x=%d, xenl=%d, width=%d} after C0/C1 %d",
      b.cur.x(), b.cur.xenl(), b.width(), uchar);
#endif
  }

  void term_t::process_escape_sequence(sequence const& seq) {
    if (seq.parameter_size() == 0) {
      switch (seq.final()) {
      case ascii_7: do_decsc(*this); return;
      case ascii_8: do_decrc(*this); return;
      case ascii_equals : do_deckpam(*this); return;
      case ascii_greater: do_deckpnm(*this); return;
      }
    } else if (seq.parameter_size() == 1) {
      switch (seq.parameter()[0]) {
      case ascii_sp:
        switch (seq.final()) {
        case ascii_F: do_s7c1t(*this); return;
        case ascii_G: do_s8c1t(*this); return;
        }
        break;
      case ascii_number:
        switch (seq.final()) {
        case ascii_8: do_decaln(*this); return;
        }
        break;
      }
    }
    print_unrecognized_sequence(seq);
  }

  void term_t::process_command_string(sequence const& seq) {
    auto _check2 = [&seq] (char32_t a, char32_t b) {
      return seq.parameter_size() >= 2 && seq.parameter()[0] == a && seq.parameter()[1] == b;
    };

    if (seq.type() == ascii_osc) {
      if (_check2(ascii_0, ascii_semicolon)) {
        m_state.title = std::u32string(seq.parameter() + 2, (std::size_t) seq.parameter_size() - 2);
        return;
      }
    }
    if (seq.type() == ascii_dcs) {
      if (_check2(ascii_dollar, ascii_q)) {
        if (do_decrqss(*this, seq.parameter() + 2, seq.parameter_size() - 2)) return;
      }
    }
    print_unrecognized_sequence(seq);
  }

  class term_input_key_impl {
    term_t& term;
    tstate_t const& s;
    bool local_echo;
    key_t mod;
    int modification_level = 2;
    int function_shift = 0;
  public:
    term_input_key_impl(term_t& term): term(term), s(term.state()) {}

  private:
    bool send_byte(byte code) {
      term.input_byte(code);
      term.input_flush(local_echo);
      return true;
    }
    bool send_char(std::uint32_t code) {
      term.input_uchar(code);
      term.input_flush(local_echo);
      return true;
    }
    bool send_csi_key(byte final_char) {
      term.input_c1(ascii_csi);
      term.input_byte(final_char);
      term.input_flush(local_echo);
      return true;
    }
    bool send_esc_key(byte final_char) {
      term.input_byte(ascii_esc);
      term.input_byte(final_char);
      term.input_flush(local_echo);
      return true;
    }
    bool send_key_modified(byte introducer, int param, byte final_char) {
      if (mod) {
        term.input_c1(modification_level >= 1 ? (byte) ascii_csi : introducer);
        if (modification_level >= 3)
          term.input_byte(ascii_greater);
        if (param) {
          term.input_unsigned(param);
          term.input_byte(ascii_semicolon);
        } else if (modification_level >= 2) {
          term.input_byte(ascii_1);
          term.input_byte(ascii_semicolon);
        }
        term.input_modifier(mod);
      } else {
        term.input_c1(introducer);
        if (param)
          term.input_unsigned(param);
      }
      term.input_byte(final_char);
      term.input_flush(local_echo);
      return true;
    }
    // Normal/Application で CSI/SS3 F
    bool send_app_modified(byte final_char) {
      return send_key_modified(s.get_mode(mode_decckm) ? ascii_ss3 : ascii_csi, 0, final_char);
    }
    // Normal/Application 共に SS3 F
    bool send_ss3_modified(byte final_char) {
      return send_key_modified(ascii_ss3, 0, final_char);
    }
    // CSI Ps F
    bool send_csi_modified(int param, byte final_char) {
      return send_key_modified(ascii_csi, param, final_char);
    }
    bool send_other_tilde(key_t code) {
      term.input_c1(ascii_csi);
      if (modification_level == 3)
        term.input_byte(ascii_greater);
      term.input_byte(ascii_2);
      term.input_byte(ascii_7);
      term.input_byte(ascii_semicolon);
      term.input_modifier(mod);
      term.input_byte(ascii_semicolon);
      term.input_unsigned(code);
      term.input_byte(ascii_tilde);
      term.input_flush(local_echo);
      return true;
    }
    bool send_other_u(key_t code) {
      term.input_c1(ascii_csi);
      term.input_unsigned(code);
      if (mod) {
        term.input_byte(ascii_semicolon);
        term.input_modifier(mod);
      }
      term.input_byte(ascii_u);
      term.input_flush(local_echo);
      return true;
    }
  private:
    bool process_default_function_key(key_t code) {
      // fn key の修飾が無効化されている時、番号を修飾でシフトする。
      if (modification_level == funckey_mlevel_shift_function) {
        if (mod & modifier_shift) code += 10;
        if (mod & modifier_control) code += 20;
        mod = 0;
        if (code > key_f20)
          return send_csi_modified(41 - key_f20 + code, ascii_tilde);
      }

      switch (code) {
      case key_f1 : return send_csi_modified(11, ascii_tilde);
      case key_f2 : return send_csi_modified(12, ascii_tilde);
      case key_f3 : return send_csi_modified(13, ascii_tilde);
      case key_f4 : return send_csi_modified(14, ascii_tilde);
      case key_f5 : return send_csi_modified(15, ascii_tilde);
      case key_f6 : return send_csi_modified(17, ascii_tilde);
      case key_f7 : return send_csi_modified(18, ascii_tilde);
      case key_f8 : return send_csi_modified(19, ascii_tilde);
      case key_f9 : return send_csi_modified(20, ascii_tilde);
      case key_f10: return send_csi_modified(21, ascii_tilde);
      case key_f11: return send_csi_modified(23, ascii_tilde);
      case key_f12: return send_csi_modified(24, ascii_tilde);
      case key_f13: return send_csi_modified(25, ascii_tilde);
      case key_f14: return send_csi_modified(26, ascii_tilde);
      case key_f15: return send_csi_modified(28, ascii_tilde);
      case key_f16: return send_csi_modified(29, ascii_tilde);
      case key_f17: return send_csi_modified(31, ascii_tilde);
      case key_f18: return send_csi_modified(32, ascii_tilde);
      case key_f19: return send_csi_modified(33, ascii_tilde);
      case key_f20: return send_csi_modified(34, ascii_tilde);
      case key_f21: return send_csi_modified(42, ascii_tilde);
      case key_f22: return send_csi_modified(43, ascii_tilde);
      case key_f23: return send_csi_modified(44, ascii_tilde);
      case key_f24: return send_csi_modified(45, ascii_tilde);
      case key_insert: return send_csi_modified(2, ascii_tilde);
      case key_delete: return send_csi_modified(3, ascii_tilde);
      case key_home  : return send_csi_modified(1, ascii_tilde);
      case key_end   : return send_csi_modified(4, ascii_tilde);
      case key_prior : return send_csi_modified(5, ascii_tilde);
      case key_next  : return send_csi_modified(6, ascii_tilde);
      case key_up    : return send_app_modified(ascii_A);
      case key_down  : return send_app_modified(ascii_B);
      case key_right : return send_app_modified(ascii_C);
      case key_left  : return send_app_modified(ascii_D);
      case key_begin : return send_app_modified(ascii_E);

      case key_focus :
        if (s.get_mode(mode_XtermAnyEventMouse))
          return send_ss3_modified(ascii_I);
        break;
      case key_blur  :
        if (s.get_mode(mode_XtermAnyEventMouse))
          return send_ss3_modified(ascii_O);
        break;

      case key_kpmul: return send_ss3_modified(ascii_j);
      case key_kpadd: return send_ss3_modified(ascii_k);
      case key_kpsep: return send_ss3_modified(ascii_l);
      case key_kpsub: return send_ss3_modified(ascii_m);
      case key_kpdec: return send_ss3_modified(ascii_n);
      case key_kpdiv: return send_ss3_modified(ascii_o);
      case key_kp0  : return send_ss3_modified(ascii_p);
      case key_kp1  : return send_ss3_modified(ascii_q);
      case key_kp2  : return send_ss3_modified(ascii_r);
      case key_kp3  : return send_ss3_modified(ascii_s);
      case key_kp4  : return send_ss3_modified(ascii_t);
      case key_kp5  : return send_ss3_modified(ascii_u);
      case key_kp6  : return send_ss3_modified(ascii_v);
      case key_kp7  : return send_ss3_modified(ascii_w);
      case key_kp8  : return send_ss3_modified(ascii_x);
      case key_kp9  : return send_ss3_modified(ascii_y);
      case key_kpeq : return send_ss3_modified(ascii_X);
      case key_kpent: return send_ss3_modified(ascii_M);
      }
      return false;
    }

    // xterm default
    bool process_pc_style_function_key(key_t code) {
      if (key_f1 <= code && code <= key_f4) {
        bool use_tilde = false;
        if (modification_level == funckey_mlevel_shift_function) {
          if (mod & (modifier_shift | modifier_control))
            use_tilde = true;
          else
            mod = 0;
        }

        if (!use_tilde) {
          switch (code) {
          case key_f1: return send_ss3_modified(ascii_P);
          case key_f2: return send_ss3_modified(ascii_Q);
          case key_f3: return send_ss3_modified(ascii_R);
          case key_f4: return send_ss3_modified(ascii_S);
          }
        }
      } else {
        switch (code) {
        case key_home  : return send_app_modified(ascii_H);
        case key_end   : return send_app_modified(ascii_F);
        }
      }
      return process_default_function_key(code);
    }

    // Mode ?1051 (Sun function-key mode)
    bool process_sun_style_function_key(key_t code) {
      if (key_f1 <= code && code < key_f24) {
        if (modification_level == funckey_mlevel_shift_function) mod = 0;
        if (code <= key_f10)
          return send_csi_modified(224 - key_f1 + code, ascii_z);
        else if (code <= key_f20)
          return send_csi_modified(182 - key_f11 + code, ascii_z);
        else
          return send_csi_modified(208 - key_f21 + code, ascii_z);
      }
      switch (code) {
      case key_insert: return send_csi_modified(2  , ascii_z);
      case key_delete: return send_csi_modified(3  , ascii_z);
      case key_home  : return send_csi_modified(214, ascii_z);
      case key_end   : return send_csi_modified(220, ascii_z);
      case key_prior : return send_csi_modified(222, ascii_z);
      case key_next  : return send_csi_modified(216, ascii_z);
      }
      return process_pc_style_function_key(code);
    }

    // Mode ?1052 (HP function-key mode)
    bool process_hp_style_function_key(key_t code) {
      if (mod == 0) {
        switch (code) {
        case key_f1    : return send_esc_key(ascii_p);
        case key_f2    : return send_esc_key(ascii_q);
        case key_f3    : return send_esc_key(ascii_r);
        case key_f4    : return send_esc_key(ascii_s);
        case key_f5    : return send_esc_key(ascii_t);
        case key_f6    : return send_esc_key(ascii_u);
        case key_f7    : return send_esc_key(ascii_v);
        case key_f8    : return send_esc_key(ascii_w);
        case key_up    : return send_esc_key(ascii_A);
        case key_down  : return send_esc_key(ascii_B);
        case key_right : return send_esc_key(ascii_C);
        case key_left  : return send_esc_key(ascii_D);
        case key_insert: return send_esc_key(ascii_Q);
        case key_delete: return send_esc_key(ascii_P);
        case key_home  : return send_esc_key(ascii_h);
        case key_end   : return send_esc_key(ascii_F);
        case key_prior : return send_esc_key(ascii_T);
        case key_next  : return send_esc_key(ascii_S);
        }
      }
      return process_pc_style_function_key(code);
    }

    // Mode ?1053 (SCO function-key mode)
    bool process_sco_style_function_key(key_t code) {
      if (mod == 0) {
        if (key_f1 <= code && code <= key_f24) {
          if (code <= key_f14)
            return send_csi_key(ascii_M - key_f1 + code);
          else
            return send_csi_key(ascii_a - key_f15 + code);
        }
        switch (code) {
        case key_up    : return send_esc_key(ascii_A);
        case key_down  : return send_esc_key(ascii_B);
        case key_right : return send_esc_key(ascii_C);
        case key_left  : return send_esc_key(ascii_D);
        case key_insert: return send_esc_key(ascii_L);
        case key_delete: return send_byte(ascii_del);
        case key_home  : return send_esc_key(ascii_H);
        case key_end   : return send_esc_key(ascii_F);
        case key_prior : return send_esc_key(ascii_I);
        case key_next  : return send_esc_key(ascii_G);
        }
      }
      return process_pc_style_function_key(code);
    }

    // Mode ?1060 (X11R6 Legacy mode)
    bool process_x11r6_style_function_key(key_t code) {
      if ((mod & modifier_control) &&
        key_f1 <= code && code <= key_f10
      ) {
        mod &= ~modifier_control;
        code += 10;
      } else {
        switch (code) {
        case key_delete: if (mod == 0) return send_byte(ascii_del); break;
        case key_f1 : return send_csi_modified(11, ascii_tilde);
        case key_f2 : return send_csi_modified(12, ascii_tilde);
        case key_f3 : return send_csi_modified(13, ascii_tilde);
        case key_f4 : return send_csi_modified(14, ascii_tilde);
        }
      }
      return process_pc_style_function_key(code);
    }

    // Mode ?1061 (VT220 Legacy mode)
    bool process_vt220_style_function_key(key_t code) {
      if ((mod & modifier_control) &&
        key_f1 <= code && code <= key_f10
      ) {
        mod &= ~modifier_control;
        code += 10;
      } else if (code == key_home) {
        return send_csi_modified(1, ascii_tilde);
      } else if (code == key_end) {
        return send_csi_modified(4, ascii_tilde);
      }
      return process_pc_style_function_key(code);
    }

  public:
    bool process(key_t key) {
      if (s.get_mode(mode_kam)) return false;
      if (key & modifier_autorepeat) {
        if (!s.get_mode(mode_decarm)) return true;
        key &= ~modifier_autorepeat;
      }

      mod = key & _modifier_mask;
      key_t code = key & _character_mask;

      // Meta は一律で ESC にする。
      local_echo = !s.get_mode(mode_srm);
      if (local_echo) term.input_flush();
      if (mod & modifier_meta) {
        term.input_byte(ascii_esc);
        mod &= ~modifier_meta;
      }

      // テンキーの文字 (!DECNKM の時)
      if (!s.get_mode(ansi::mode_decnkm)) {
        switch (code) {
        case key_kp0  : code = ascii_0; break;
        case key_kp1  : code = ascii_1; break;
        case key_kp2  : code = ascii_2; break;
        case key_kp3  : code = ascii_3; break;
        case key_kp4  : code = ascii_4; break;
        case key_kp5  : code = ascii_5; break;
        case key_kp6  : code = ascii_6; break;
        case key_kp7  : code = ascii_7; break;
        case key_kp8  : code = ascii_8; break;
        case key_kp9  : code = ascii_9; break;
        case key_kpdec: code = ascii_dot     ; break;
        case key_kpsep: code = ascii_comma   ; break;
        case key_kpmul: code = ascii_asterisk; break;
        case key_kpadd: code = ascii_plus    ; break;
        case key_kpsub: code = ascii_minus   ; break;
        case key_kpdiv: code = ascii_slash   ; break;
        case key_kpeq : code = ascii_equals  ; break;
        case key_kpent: code = ascii_cr      ; break;
        default: ;
        }
      }

      // modifyOtherKeys 設定
      if (mod) {
        if (_key_cursor_first <= code && code <= _key_cursor_last) {
          modification_level = s.m_funckey_flags >> funckey_modifyCursorKeys_shift & funckey_mlevel_mask;
        } else if (_key_fn_first <= code && code <= _key_fn_last) {
          modification_level = s.m_funckey_flags >> funckey_modifyFunctionKeys_shift & funckey_mlevel_mask;
          if (modification_level == funckey_mlevel_none)
            modification_level = funckey_mlevel_shift_function;
        } else if (_key_kp_first <= code && code <= _key_kp_last) {
          modification_level = s.m_funckey_flags >> funckey_modifyKeypadKeys_shift & funckey_mlevel_mask;
        } else {
          modification_level = s.m_funckey_flags >> funckey_modifyOtherKeys_shift & funckey_mlevel_mask;
        }
      }

      // C0 文字および DEL (C-@...C-_, C-back)
      if (modification_level == funckey_mlevel_none)
        mod &= modifier_control;
      if (mod == modifier_control) {
        if (code == ascii_sp ||
          (ascii_a <= code && code <= ascii_z) ||
          (ascii_left_bracket <= code && code <= ascii_underscore)
        ) {
          // C-@ ... C-_
          return send_byte(code & 0x1F);
        } else if (code == ascii_question) {
          // C-? → ^? (DEL 0x7F)
          return send_byte(ascii_del);
        } else if (code == ascii_bs) {
          // C-back → ^_ (US 0x1F)
          return send_byte(ascii_us);
        }
      }
      if (modification_level == funckey_mlevel_none) mod = 0;

      // 通常文字 および BS CR(RET) HT(TAB) ESC
      if (code < _key_base) {
        if (mod == 0) {
          // Note: ESC, RET, HT はそのまま (C-[, C-m, C-i) 送信される。
          if (code == ascii_bs && !s.get_mode(mode_decbkm)) code = ascii_del;
          return send_char(code);
        } else if (code < _key_base) {
          // CSI <Unicode> ; <Modifier> u 形式
          //return send_other_u(code);

          // CSI 27 ; <Modifier> ; <Unicode> ~ 形式
          return send_other_tilde(code);
        }
      }

      switch (s.m_funckey_flags & funckey_mode_mask) {
      case funckey_sun:   return process_sun_style_function_key(code);
      case funckey_hp:    return process_hp_style_function_key(code);
      case funckey_sco:   return process_sco_style_function_key(code);
      case funckey_x11r6: return process_x11r6_style_function_key(code);
      case funckey_vt220: return process_vt220_style_function_key(code);
      case funckey_pc:    return process_pc_style_function_key(code);
      default: return process_default_function_key(code);
      }
    }
  };
  bool term_t::input_key(key_t key) {
    return term_input_key_impl(*this).process(key);
  }

  bool term_t::input_mouse(key_t key, [[maybe_unused]] coord_t px, [[maybe_unused]] coord_t py, curpos_t const x, curpos_t const y) {
    tstate_t const& s = this->state();
    if (!(s.mouse_mode & mouse_report_mask)) return false;

    std::uint32_t button = 0;
    switch (key & _key_mouse_button_mask) {
    case _key_mouse1: button = 0;  break;
    case _key_mouse2: button = 1;  break;
    case _key_mouse3: button = 2;  break;
    case _key_mouse4: button = 66; break; // contra: 適当
    case _key_mouse5: button = 67; break; // contra: 適当
    case key_wheel_up:   button = 64; break;
    case key_wheel_down: button = 65; break;
    case 0: button = 3; break;
    default: return false;
    }

    char final_character = ascii_M;

    switch (key & _key_mouse_event_mask) {
    case _key_mouse_event_down:
      if (s.mouse_mode & mouse_report_down)
        goto report_sequence;
      break;
    case _key_mouse_event_up:
      if (s.mouse_mode & mouse_report_up) {
        if (s.mouse_mode & mouse_sequence_sgr)
          final_character = ascii_m;
        else
          button = 3;
        goto report_sequence;
      } else if (s.mouse_mode & mouse_report_select) {
        final_character = ascii_t;
        goto report_sequence;
      }
      break;
    case _key_mouse_event_move:
      // 前回と同じ升目に居る時は送らなくて良い。
      if (x == m_mouse_prev_x && y == m_mouse_prev_y) return true;
      if (key & _key_mouse_button_mask) {
        if (s.mouse_mode & mouse_report_drag) {
          button |= 32;
          goto report_sequence;
        }
      } else if (s.mouse_mode & mouse_report_move) {
        button |= 32;
        goto report_sequence;
      }
      break;
    report_sequence:
      if (key & modifier_shift) button |= 4;
      if (key & modifier_meta) button |= 8;
      if (key & modifier_control) button |= 16;

      switch (std::uint32_t const seqtype = s.mouse_mode & mouse_sequence_mask) {
      default:
        input_c1(ascii_csi);
        input_byte(final_character);
        if (final_character != ascii_t)
          input_byte(std::min(button + 32, 255u));
        input_byte(std::clamp(x + 33, 0, 255));
        input_byte(std::clamp(y + 33, 0, 255));
        break;
      case mouse_sequence_utf8:
        input_c1(ascii_csi);
        input_byte(final_character);
        if (final_character != ascii_t)
          input_uchar(std::min(button + 32, (std::uint32_t) _unicode_max));
        input_uchar(std::clamp<curpos_t>(x + 33, 0, _unicode_max));
        input_uchar(std::clamp<curpos_t>(y + 33, 0, _unicode_max));
        break;
      case mouse_sequence_urxvt:
      case mouse_sequence_sgr:
        input_c1(ascii_csi);
        if (seqtype == mouse_sequence_sgr)
          input_byte(ascii_less);
        if (final_character != ascii_t) {
          input_unsigned(button);
          input_byte(ascii_semicolon);
        }
        input_unsigned(x + 1);
        input_byte(ascii_semicolon);
        input_unsigned(y + 1);
        input_byte(final_character);
        break;
      }
      input_flush();
      m_mouse_prev_x = x;
      m_mouse_prev_y = y;
      return true;
    }

    return false;
  }

  bool term_t::input_paste(std::u32string const& data) {
    // Note: data が空であったとしても paste_begin/end は送る事にする。
    //   空文字列でも vim 等でモードの変更などを引き起こしそうな気がする。
    bool const bracketed_paste_mode = state().get_mode(mode_XtermPasteInBracket);
    if (bracketed_paste_mode) {
      input_c1(ascii_csi);
      input_unsigned(200);
      input_byte(ascii_tilde);
      for (char32_t const u : data) input_uchar(u);
      input_c1(ascii_csi);
      input_unsigned(201);
      input_byte(ascii_tilde);
    } else {
      // 改行 LF は RET に書き換える。
      for (char32_t const u : data)
        input_uchar(u == '\n' ? '\r' : u);
    }
    input_flush();
    return true;
  }

  void frame_snapshot_list::remove(frame_snapshot_t* snapshot) {
    m_data.erase(std::remove(m_data.begin(), m_data.end(), snapshot), m_data.end());
  }
  void frame_snapshot_list::add(frame_snapshot_t* snapshot) {
    m_data.push_back(snapshot);
  }
  frame_snapshot_list::~frame_snapshot_list() {
    std::vector<frame_snapshot_t*> snapshots(std::move(m_data));
    for (frame_snapshot_t* snapshot: snapshots) snapshot->reset();
  }

  void term_t::gc(std::uint32_t threshold) {
    if (m_atable.gc_count() < threshold) return;
    m_board.gc_mark();
    m_scroll_buffer.gc_mark();
    state().m_decsc_cur.abuild.gc_mark();
    state().altscreen.gc_mark();
    for (frame_snapshot_t* snapshot: m_snapshots.m_data)
      snapshot->gc_mark(m_atable);
    m_atable.sweep();
  }

}
}
