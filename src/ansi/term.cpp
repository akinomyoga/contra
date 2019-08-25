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

namespace contra {
namespace ansi {

  typedef bool control_function_t(term_t& term, csi_parameters& params);

  static void do_hvp_impl(term_t& term, curpos_t x, curpos_t y);
  static void do_vertical_scroll(term_t& term, curpos_t shift, curpos_t tmargin, curpos_t bmargin, curpos_t lmargin, curpos_t rmargin, bool dcsm, bool transfer);
  static void do_vertical_scroll(term_t& term, curpos_t shift, bool dcsm);

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

    void set_ansi_mode(tstate_t& s, csi_single_param_t param, bool value) {
      auto const it = data_ansi.find(param);
      if (it != data_ansi.end())
        s.set_mode(it->second, value);
      else
        std::fprintf(stderr, "unrecognized ANSI mode %u\n", (unsigned) param);
    }
    void set_dec_mode(tstate_t& s, csi_single_param_t param, bool value) {
      auto const it = data_dec.find(param);
      if (it != data_dec.end())
        s.set_mode(it->second, value);
      else
        std::fprintf(stderr, "unrecognized DEC mode %u\n", (unsigned) param);
    }
    int rqm_ansi_mode(tstate_t& s, csi_single_param_t param) {
      auto const it = data_ansi.find(param);
      return it == data_ansi.end() ? 0 : s.rqm_mode(it->second);
    }
    int rqm_dec_mode(tstate_t& s, csi_single_param_t param) {
      auto const it = data_dec.find(param);
      return it == data_dec.end() ? 0 : s.rqm_mode(it->second);
    }
  };
  static mode_dictionary_t mode_dictionary;

  bool do_sm(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_ansi_mode(s, value, true);
    return true;
  }
  bool do_rm(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_ansi_mode(s, value, false);
    return true;
  }
  bool do_decset(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_dec_mode(s, value, true);
    return true;
  }
  bool do_decrst(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_dec_mode(s, value, false);
    return true;
  }
  static bool do_decrqm_impl(term_t& term, csi_parameters& params, bool decmode) {
    tstate_t& s = term.state();
    csi_single_param_t value;
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
  bool do_decrqm_ansi(term_t& term, csi_parameters& params) {
    return do_decrqm_impl(term, params, false);
  }
  bool do_decrqm_dec(term_t& term, csi_parameters& params) {
    return do_decrqm_impl(term, params, false);
  }

  bool do_decscusr(term_t& term, csi_parameters& params) {
    csi_single_param_t spec;
    params.read_param(spec, 0);
    tstate_t& s = term.state();
    auto _set = [&s] (bool blink, int shape) {
      s.set_mode(mode_attCursorBlink, blink);
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

  void do_altscreen(term_t& term, bool value) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    if (value != s.get_mode(mode_altscr)) {
      s.set_mode(mode_altscr, value);
      s.altscreen.reset_size(b.m_width, b.m_height);
      s.altscreen.cur = b.cur;
      s.altscreen.m_line_count = b.m_line_count;
      std::swap(s.altscreen, b);
    }
  }

  void do_deckpam(term_t& term) {
    term.state().set_mode(mode_decnkm, true);
  }
  void do_deckpnm(term_t& term) {
    term.state().set_mode(mode_decnkm, false);
  }

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

  void do_sm_deccolm(term_t& term, bool value) {
    tstate_t& s = term.state();
    if (s.get_mode(mode_xtEnableColm)) {
      board_t& b = term.board();
      b.reset_size(value ? 132 : 80, b.m_height);
      if (!s.get_mode(mode_decncsm)) b.clear_screen();
      s.clear_margin();
      s.set_mode(mode_declrmm, false);
      b.cur.set(0, 0);
    }
  }
  int do_rqm_deccolm(term_t& term) {
    return term.board().m_width == 132 ? 1 : 2;
  }
  bool do_decscpp(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    if (!s.cfg_decscpp_enabled) return true;

    csi_single_param_t param;
    params.read_param(param, 0);
    if (param == 0) param = 80;
    curpos_t cols = contra::clamp<curpos_t>(param, s.cfg_decscpp_min, s.cfg_decscpp_max);

    board_t& b = term.board();
    b.reset_size(cols, b.m_height);

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
    // Note: mode_xenl, mode_decawm, mode_xtBSBackLine が絡んで来た時の振る舞いは適当である。
    board_t& b = term.board();
    tstate_t const& s = term.state();
    line_t const& line = b.line();
    if (s.get_mode(mode_simd)) {
      bool const cap_xenl = s.get_mode(mode_xenl);
      curpos_t sll = term.implicit_sll(line);
      curpos_t x = b.cur.x(), y = b.cur.y();
      bool xenl = false;
      if (b.cur.xenl() || (!cap_xenl && (x == sll || x == b.m_width - 1))) {
        if (s.get_mode(mode_decawm) && s.get_mode(mode_xtBSBackLine) && y > 0) {
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

      mwg_assert(x < b.m_width);
      if (x != sll && x != b.m_width - 1) {
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
      } else if (s.get_mode(mode_decawm) && s.get_mode(mode_xtBSBackLine) && y > 0) {
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
    if (b.cur.attribute.xflags & attribute_t::decdhl_mask) char_width *= 2;
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
      sll = b.m_width - 1;
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
    cell.attribute = b.cur.attribute;
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
    int const width_multiplier = b.cur.attribute.xflags & attribute_t::decdhl_mask ? 2 : 1;

    curpos_t x = b.cur.x();

    // current line and range
    line_t* line = &b.line();
    term.initialize_line(*line);
    curpos_t slh = term.implicit_slh(*line);
    curpos_t sll = term.implicit_sll(*line);
    if (x < slh)
      slh = 0;
    else if (x > (b.cur.xenl() ? sll + 1 : sll))
      sll = b.m_width - 1;
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
    cell.attribute = b.cur.attribute;
    cell.width = 1;

    std::vector<cell_t>& buffer = term.m_buffer;
    mwg_assert(buffer.empty());
    curpos_t xwrite = 0;
    auto _write = [&] () {
      if (simd) std::reverse(buffer.begin(), buffer.end());
      line->write_cells(xwrite, &buffer[0], buffer.size(), 1, dir);
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
      if (simd)
        xwrite = std::max(0, x - (char_width - 1));
      else if (buffer.empty())
        xwrite = x;
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
    csi_single_param_t direction;
    params.read_param(direction, 0);
    csi_single_param_t update;
    params.read_param(update, 0);
    if (direction > 7 || update > 2) return false;

    board_t& b = term.board();

    bool const oldRToL = is_charpath_rtol(b.m_presentation_direction);
    bool const newRToL = is_charpath_rtol(b.m_presentation_direction = (presentation_direction_t) direction);

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
      for (curpos_t y = 0, yN = b.m_height; y < yN; y++) {
        line_t& line = b.m_lines[y];
        if (!(line.lflags() & line_attr_t::character_path_mask))
          line.reverse(b.m_width);
      }
    }

    // update position
    b.cur.set(update == 2 ? b.to_data_position(b.cur.y(), 0) : 0, 0);
    return true;
  }

  bool do_scp(term_t& term, csi_parameters& params) {
    csi_single_param_t charPath;
    params.read_param(charPath, 0);
    csi_single_param_t update;
    params.read_param(update, 0);
    if (charPath > 2 || update > 2) return false;

    tstate_t& s = term.state();
    board_t& b = term.board();
    line_t& line = b.line();

    bool const oldRToL = line.is_r2l(b.m_presentation_direction);

    // update line attributes
    term.initialize_line(line);
    line.lflags() &= ~line_attr_t::character_path_mask;
    s.lflags &= ~line_attr_t::character_path_mask;
    switch (charPath) {
    case 1:
      line.lflags() |= line_attr_t::is_character_path_ltor;
      s.lflags |= line_attr_t::is_character_path_ltor;
      break;
    case 2:
      line.lflags() |= line_attr_t::is_character_path_rtol;
      s.lflags |= line_attr_t::is_character_path_rtol;
      break;
    }

    bool const newRToL = line.is_r2l(b.m_presentation_direction);

    // update line content
    if (oldRToL != newRToL && update == 2)
      b.line().reverse(b.m_width);

    b.cur.set_x(update == 2 ? b.to_data_position(b.cur.y(), 0) : 0);
    return true;
  }

  bool do_simd(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 1) return false;
    term.state().set_mode(mode_simd, param != 0);
    return true;
  }

  bool do_slh(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    line_t& line = b.line();

    csi_single_param_t param;
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

    csi_single_param_t param;
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

    csi_single_param_t param;
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

    csi_single_param_t param;
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

    csi_single_param_t param1, param2;
    params.read_param(param1, 0);
    params.read_param(param2, 0);

    curpos_t const home = param1 == 0 ? 0 : std::min((curpos_t) param1 - 1, b.m_height);
    curpos_t const limit = param2 == 0 ? b.m_height : std::min((curpos_t) param2, b.m_height);
    if (home + 2 <= limit) {
      s.dec_tmargin = home == 0 ? -1 : home;
      s.dec_bmargin = limit == b.m_height ? -1 : limit;
      do_hvp_impl(term, 0, 0);
    }
    return true;
  }

  bool do_decslrm(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    if (!s.get_mode(mode_declrmm)) return true;

    csi_single_param_t param1, param2;
    params.read_param(param1, 0);
    params.read_param(param2, 0);

    curpos_t const _min = param1 == 0 ? 0 : std::min((curpos_t) param1 - 1, b.m_width);
    curpos_t const _max = param2 == 0 ? b.m_width : std::min((curpos_t) param2, b.m_width);
    if (_min + 2 <= _max) {
      s.dec_lmargin = _min == 0 ? -1 : _min;
      s.dec_rmargin = _max == b.m_width ? -1 : _max;
      do_hvp_impl(term, 0, 0);
    }
    return true;
  }

  //---------------------------------------------------------------------------
  // Strings

  bool do_sds(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;
    term.insert_marker(
      param == 1 ? marker_sds_l2r :
      param == 2 ? marker_sds_r2l :
      marker_sds_end);
    return true;
  }
  bool do_srs(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;
    term.insert_marker(
      param == 1 ? marker_srs_beg :
      marker_srs_end);
    return true;
  }

  //---------------------------------------------------------------------------
  // Cursor

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

  static void do_cux(term_t& term, csi_single_param_t param, do_cux_direction direction, bool isPresentation, bool check_stbm) {
    board_t& b = term.board();

    curpos_t y = b.cur.y();
    switch (direction) {
    case do_cux_prec_char:
      {
        curpos_t lmargin = 0;
        if (check_stbm) {
          if (is_charpath_rtol(b.m_presentation_direction))
            lmargin = std::max((curpos_t) 0, b.m_width - term.rmargin());
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
        curpos_t rmargin = term.board().m_width;
        if (check_stbm) {
          if (is_charpath_rtol(b.m_presentation_direction))
            rmargin = std::max((curpos_t) 0, b.m_width - term.lmargin());
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
      b.cur.set_y(std::min(y + (curpos_t) param, (check_stbm ? term.bmargin() : term.board().m_height) - 1));
      break;
    }
  }
  static bool do_cux(term_t& term, csi_parameters& params, do_cux_direction direction, bool isPresentation, bool check_stbm) {
    tstate_t& s = term.state();
    csi_single_param_t param;
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
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_u, term.board().m_presentation_direction);
    return do_cux(term, params, dir, !term.state().get_mode(mode_bdsm), true);
  }
  bool do_cud(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_d, term.board().m_presentation_direction);
    return do_cux(term, params, dir, !term.state().get_mode(mode_bdsm), true);
  }
  bool do_cuf(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_r, term.board().m_presentation_direction);
    return do_cux(term, params, dir, !term.state().get_mode(mode_bdsm), true);
  }
  bool do_cub(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_l, term.board().m_presentation_direction);
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
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0 && s.get_mode(mode_zdm)) param = 1;

    board_t& b = term.board();
    curpos_t const y = std::min(b.cur.y() + (curpos_t) param, term.bmargin() - 1);
    return do_cup(term, 0, y);
  }

  bool do_cpl(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0 && s.get_mode(mode_zdm)) param = 1;

    board_t& b = term.board();
    curpos_t const y = std::max(b.cur.y() - (curpos_t) param, term.tmargin());
    return do_cup(term, 0, y);
  }

  bool do_cup(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t param1, param2;
    params.read_param(param1, 1);
    params.read_param(param2, 1);
    if (s.get_mode(mode_zdm)) {
      if (param1 == 0) param1 = 1;
      if (param2 == 0) param2 = 1;
    }

    if (param1 == 0 || param2 == 0) return false;

    if (s.get_mode(mode_decom)) {
      param1 = std::min<csi_single_param_t>(param1 + term.tmargin(), term.bmargin());
      param2 = std::min<csi_single_param_t>(param2 + term.lmargin(), term.rmargin());
    } else {
      board_t& b = term.board();
      param1 = std::min<csi_single_param_t>(param1, b.m_height);
      param2 = std::min<csi_single_param_t>(param2, b.m_width);
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
      if (x >= b.m_width) x = b.m_width - 1;
      if (y >= b.m_height) y = b.m_height - 1;
    }
    b.cur.set(x, y);
  }
  bool do_hvp(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t param1, param2;
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
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    board_t& b = term.board();
    if (s.get_mode(mode_decom))
      param = std::min<csi_single_param_t>(param + term.lmargin(), term.rmargin());
    else
      param = std::min<csi_single_param_t>(param, b.m_width);
    return do_cup(term, param - 1, b.cur.y());
  }

  bool do_hpa(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    board_t& b = term.board();
    if (s.get_mode(mode_decom))
      param = std::min<csi_single_param_t>(param + term.lmargin(), term.rmargin());
    else
      param = std::min<csi_single_param_t>(param, b.m_width);
    b.cur.set_x(param - 1);
    return true;
  }

  bool do_vpa(term_t& term, csi_parameters& params) {
    tstate_t& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    board_t& b = term.board();
    if (s.get_mode(mode_decom))
      param = std::min<csi_single_param_t>(param + term.tmargin(), term.bmargin());
    else
      param = std::min<csi_single_param_t>(param, b.m_height);

    b.cur.adjust_xenl();
    b.cur.set_y(param - 1);
    return true;
  }

  static void do_vertical_scroll(term_t& term, curpos_t shift, curpos_t tmargin, curpos_t bmargin, curpos_t lmargin, curpos_t rmargin, bool dcsm, bool transfer) {
    board_t& b = term.board();
    if (lmargin == 0 && rmargin == b.m_width) {
      b.shift_lines(tmargin, bmargin, shift, term.fill_attr(), transfer ? &term.m_scroll_buffer : nullptr);
    } else if (lmargin < rmargin) {
      // DECSLRM が設定されている時のスクロール。行内容を切り貼りする。
      line_segment_t segs[3];
      int iseg = 0, iseg_transfer;
      if (0 < lmargin)
        segs[iseg++] = line_segment_t({0, lmargin, line_segment_slice});
      segs[iseg_transfer = iseg++] = line_segment_t({lmargin, rmargin, line_segment_transfer});
      if (rmargin < b.m_width)
        segs[iseg++] = line_segment_t({rmargin, b.m_width, line_segment_slice});

      attribute_t const fill_attr = term.fill_attr();
      if (shift > 0) {
        curpos_t ydst = bmargin - 1;
        curpos_t ysrc = ydst - shift;
        for (; tmargin <= ysrc; ydst--, ysrc--) {
          segs[iseg_transfer].source = &b.line(ysrc);
          segs[iseg_transfer].source_r2l = b.line_r2l(ysrc);
          b.line(ydst).compose_segments(segs, iseg, b.m_width, fill_attr, b.line_r2l(ydst), dcsm);
        }
        segs[iseg_transfer].type = line_segment_erase;
        for (; tmargin <= ydst; ydst--)
          b.line(ydst).compose_segments(segs, iseg, b.m_width, fill_attr, b.line_r2l(ydst), dcsm);
      } else if (shift < 0) {
        shift = -shift;
        curpos_t ydst = tmargin;
        curpos_t ysrc = tmargin + shift;
        for (; ysrc < bmargin; ydst++, ysrc++) {
          segs[iseg_transfer].source = &b.line(ysrc);
          segs[iseg_transfer].source_r2l = b.line_r2l(ysrc);
          b.line(ydst).compose_segments(segs, iseg, b.m_width, fill_attr, b.line_r2l(ydst), dcsm);
        }
        segs[iseg_transfer].type = line_segment_erase;
        for (; ydst < bmargin; ydst++)
          b.line(ydst).compose_segments(segs, iseg, b.m_width, fill_attr, b.line_r2l(ydst), dcsm);
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
    csi_single_param_t param;
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
        line_shift_flags flags = b.line_r2l() ? line_shift_flags::r2l : line_shift_flags::none;
        if (dcsm) flags |= line_shift_flags::dcsm;
        curpos_t const tmargin = term.tmargin();
        curpos_t const bmargin = term.bmargin();
        curpos_t const lmargin = term.lmargin();
        curpos_t const rmargin = term.rmargin();
        attribute_t const fill_attr = term.fill_attr();
        for (curpos_t y = tmargin; y < bmargin; y++)
          b.line(y).shift_cells(lmargin, rmargin, shift, flags, b.m_width, fill_attr);
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
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_u, term.board().m_presentation_direction);
    return do_scroll(term, params, dir);
  }
  bool do_sd(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_d, term.board().m_presentation_direction);
    return do_scroll(term, params, dir);
  }
  bool do_sr(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_r, term.board().m_presentation_direction);
    return do_scroll(term, params, dir);
  }
  bool do_sl(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_l, term.board().m_presentation_direction);
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
    if (y >= b.m_height) y =  b.m_height - 1;
    if (xenl) {
      if (x >= b.m_width) {
        x = b.m_width;
      } else if (x != term.implicit_sll(b.line(y)) + 1) {
        x++;
        xenl = false;
      }
    } else {
      if (x >= b.m_width) x = b.m_width - 1;
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
      b.cur.attribute = s.m_decsc_cur.attribute;
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
  // SGR and other graphics

  static void do_sgr_iso8613_colors(board_t& b, csi_parameters& params, bool isfg) {
    csi_single_param_t colorSpace;
    params.read_arg(colorSpace, true, 0);
    color_t color;
    switch (colorSpace) {
    case attribute_t::color_space_default:
    default:
      if (isfg)
        b.cur.attribute.reset_fg();
      else
        b.cur.attribute.reset_bg();
      break;

    case attribute_t::color_space_transparent:
      color = 0;
      goto set_color;

    case attribute_t::color_space_rgb:
    case attribute_t::color_space_cmy:
    case attribute_t::color_space_cmyk:
      {
        color = 0;

        int const ncomp = colorSpace == attribute_t::color_space_cmyk ? 4 : 3;
        csi_single_param_t comp;
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

    case attribute_t::color_space_indexed:
      if (params.read_arg(color, true, 0))
        goto set_color;
      else
        std::fprintf(stderr, "missing argument for SGR 38:5\n");
      break;

    set_color:
      if (isfg)
        b.cur.attribute.set_fg(color, colorSpace);
      else
        b.cur.attribute.set_bg(color, colorSpace);
      break;
    }
  }

  static void do_sgr_parameter(term_t& term, csi_single_param_t param, csi_parameters& rest) {
    board_t& b = term.board();
    if (30 <= param && param < 40) {
      if (param < 38) {
        b.cur.attribute.set_fg(param - 30);
      } else if (param == 38) {
        do_sgr_iso8613_colors(b, rest, true);
      } else {
        b.cur.attribute.reset_fg();
      }
      return;
    } else if (40 <= param && param < 50) {
      if (param < 48) {
        b.cur.attribute.set_bg(param - 40);
      } else if (param == 48) {
        do_sgr_iso8613_colors(b, rest, false);
      } else {
        b.cur.attribute.reset_bg();
      }
      return;
    } else if (90 <= param && param <= 97) {
      b.cur.attribute.set_fg(8 + (param - 90));
      return;
    } else if (100 <= param && param <= 107) {
      b.cur.attribute.set_bg(8 + (param - 100));
      return;
    }

    aflags_t& aflags = b.cur.attribute.aflags;
    xflags_t& xflags = b.cur.attribute.xflags;

    typedef attribute_t _at;

    if (10 <= param && param <= 19) {
      xflags = (xflags & ~(xflags_t) _at::ansi_font_mask)
        | ((param - 10) << _at::ansi_font_shift & _at::ansi_font_mask);
      return;
    }

    switch (param) {
    case 0: b.cur.attribute.clear_sgr(); break;

    case 1 : aflags = (aflags & ~(aflags_t) _at::is_faint_set) | _at::is_bold_set; break;
    case 2 : aflags = (aflags & ~(aflags_t) _at::is_bold_set) | _at::is_faint_set; break;
    case 22: aflags = aflags & ~(aflags_t) (_at::is_bold_set | _at::is_faint_set); break;

    case 3 : aflags = (aflags & ~(aflags_t) _at::is_fraktur_set) | _at::is_italic_set; break;
    case 20: aflags = (aflags & ~(aflags_t) _at::is_italic_set) | _at::is_fraktur_set; break;
    case 23: aflags = aflags & ~(aflags_t) (_at::is_italic_set | _at::is_fraktur_set); break;

    case 4 : aflags = (aflags & ~(aflags_t) _at::is_double_underline_set) | _at::is_underline_set; break;
    case 21: aflags = (aflags & ~(aflags_t) _at::is_underline_set) | _at::is_double_underline_set; break;
    case 24: aflags = aflags & ~(aflags_t) (_at::is_underline_set | _at::is_double_underline_set); break;

    case 5 : aflags = (aflags & ~(aflags_t) _at::is_rapid_blink_set) | _at::is_blink_set; break;
    case 6 : aflags = (aflags & ~(aflags_t) _at::is_blink_set) | _at::is_rapid_blink_set; break;
    case 25: aflags = aflags & ~(aflags_t) (_at::is_blink_set | _at::is_rapid_blink_set); break;

    case 7 : aflags = aflags | _at::is_inverse_set; break;
    case 27: aflags = aflags & ~(aflags_t) _at::is_inverse_set; break;

    case 8 : aflags = aflags | _at::is_invisible_set; break;
    case 28: aflags = aflags & ~(aflags_t) _at::is_invisible_set; break;

    case 9 : aflags = aflags | _at::is_strike_set; break;
    case 29: aflags = aflags & ~(aflags_t) _at::is_strike_set; break;

    case 26: xflags = xflags | _at::is_proportional_set; break;
    case 50: xflags = xflags & ~(xflags_t) _at::is_proportional_set; break;

    case 51: xflags = (xflags & ~(xflags_t) _at::is_circle_set) | _at::is_frame_set; break;
    case 52: xflags = (xflags & ~(xflags_t) _at::is_frame_set) | _at::is_circle_set; break;
    case 54: xflags = xflags & ~(xflags_t) (_at::is_frame_set | _at::is_circle_set); break;

    case 53: xflags = xflags | _at::is_overline_set; break;
    case 55: xflags = xflags & ~(xflags_t) _at::is_overline_set; break;

    // ECMA-48:1986 SGR(60-65) 及び JIS X 0211 & ISO/IEC 6429:1992 SGR(66-69)
    case 60: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_line_single_rb; break;
    case 61: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_line_double_rb; break;
    case 62: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_line_single_lt; break;
    case 63: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_line_double_lt; break;
    case 66: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_line_single_lb; break;
    case 67: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_line_double_lb; break;
    case 68: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_line_single_rt; break;
    case 69: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_line_double_rt; break;
    case 64: xflags = (xflags & ~(xflags_t) _at::is_ideogram_mask) | _at::is_ideogram_stress   ; break;
    case 65: xflags = xflags & ~(xflags_t) _at::is_ideogram_mask; break;

    // contra 拡張 (画面の横分割に対応する為には DECDWL の類を属性として提供する必要がある)
    case 6703: xflags = (xflags & ~(xflags_t) _at::decdhl_mask) | _at::decdhl_upper_half; break;
    case 6704: xflags = (xflags & ~(xflags_t) _at::decdhl_mask) | _at::decdhl_lower_half; break;
    case 6705: xflags = (xflags & ~(xflags_t) _at::decdhl_mask) | _at::decdhl_single_width; break;
    case 6706: xflags = (xflags & ~(xflags_t) _at::decdhl_mask) | _at::decdhl_double_width; break;

    // RLogin 拡張 (RLogin では 60-65 で利用できる。JIS X 0211 の記述と異なるが便利そうなので対応する)
    case 8460: xflags = (xflags & ~(xflags_t) _at::rlogin_double_rline) | _at::rlogin_single_rline; break;
    case 8461: xflags = (xflags & ~(xflags_t) _at::rlogin_single_rline) | _at::rlogin_double_rline; break;
    case 8462: xflags = (xflags & ~(xflags_t) _at::rlogin_double_lline) | _at::rlogin_single_lline; break;
    case 8463: xflags = (xflags & ~(xflags_t) _at::rlogin_single_lline) | _at::rlogin_double_lline; break;
    case 8464: xflags = (xflags & ~(xflags_t) _at::rlogin_single_lline) | _at::rlogin_double_strike; break;
    case 8465: xflags = xflags & ~(xflags_t) _at::rlogin_ideogram_mask; break;

    default:
      std::fprintf(stderr, "unrecognized SGR value %d\n", param);
      break;
    }
  }

  bool do_sgr(term_t& term, csi_parameters& params) {
    if (params.size() == 0 || !term.state().get_mode(mode_grcm))
      do_sgr_parameter(term, 0, params);

    csi_single_param_t value;
    while (params.read_param(value, 0))
      do_sgr_parameter(term, value, params);

    return true;
  }

  bool do_sco(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 7) return false;

    xflags_t& xflags = term.board().cur.attribute.xflags;
    xflags = (xflags & ~(xflags_t) attribute_t::sco_mask) | param << attribute_t::sco_shift;
    return true;
  }

  void do_plu(term_t& term) {
    xflags_t& xflags = term.board().cur.attribute.xflags;
    if (xflags & attribute_t::is_sub_set)
      xflags &= ~(xflags_t) attribute_t::is_sub_set;
    else
      xflags |= attribute_t::is_sup_set;
  }

  void do_pld(term_t& term) {
    xflags_t& xflags = term.board().cur.attribute.xflags;
    if (xflags & attribute_t::is_sup_set)
      xflags &= ~(xflags_t) attribute_t::is_sup_set;
    else
      xflags |= attribute_t::is_sub_set;
  }

  bool do_decsca(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;

    xflags_t& xflags = term.board().cur.attribute.xflags;
    if (param == 1)
      xflags |= (xflags_t) attribute_t::decsca_protected;
    else
      xflags &= ~(xflags_t) attribute_t::decsca_protected;
    return true;
  }

  void do_spa(term_t& term) {
    term.board().cur.attribute.xflags |= (xflags_t) attribute_t::spa_protected;
  }
  void do_epa(term_t& term) {
    term.board().cur.attribute.xflags &= ~(xflags_t) attribute_t::spa_protected;
  }
  void do_ssa(term_t& term) {
    term.board().cur.attribute.xflags |= (xflags_t) attribute_t::ssa_selected;
  }
  void do_esa(term_t& term) {
    term.board().cur.attribute.xflags &= ~(xflags_t) attribute_t::ssa_selected;
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
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    board_t& b = term.board();
    line_shift_flags flags = b.line_r2l() ? line_shift_flags::r2l : line_shift_flags::none;
    if (!s.get_mode(mode_erm)) flags |= line_shift_flags::erm_protect;

    curpos_t x1;
    if (s.get_mode(mode_xenl_ech)) b.cur.adjust_xenl();
    if (s.dcsm()) {
      // DCSM(DATA)
      flags |= line_shift_flags::dcsm;
      x1 = b.cur.x();
    } else {
      // DCSM(PRESENTATION)
      x1 = b.to_presentation_position(b.cur.y(), b.cur.x());
    }
    mwg_assert(x1 <= b.m_width);

    curpos_t const x2 = std::min(x1 + (curpos_t) param, b.m_width);
    b.line().shift_cells(x1, x2, x2 - x1, flags, b.m_width, term.fill_attr());

    // カーソル位置
    if (!s.dcsm())
      x1 = b.to_data_position(b.cur.y(), x1);
    b.cur.update_x(x1);

    return true;
  }

  static void do_ich_impl(term_t& term, curpos_t shift) {
    board_t& b = term.board();
    tstate_t const& s = term.state();
    line_shift_flags flags = b.line_r2l() ? line_shift_flags::r2l : line_shift_flags::none;

    curpos_t x1;
    if (s.get_mode(mode_xenl_ech)) b.cur.adjust_xenl();
    if (s.dcsm()) {
      // DCSM(DATA)
      flags |= line_shift_flags::dcsm;
      x1 = b.cur.x();
    } else {
      // DCSM(PRESENTATION)
      x1 = b.to_presentation_position(b.cur.y(), b.cur.x());
    }
    mwg_assert(x1 <= b.m_width);

    line_t& line = b.line();
    curpos_t const slh = term.implicit_slh(line);
    curpos_t const sll = term.implicit_sll(line);

    if (!s.get_mode(mode_hem))
      line.shift_cells(x1, sll + 1, shift, flags, b.m_width, term.fill_attr());
    else
      line.shift_cells(slh, x1 + 1, -shift, flags, b.m_width, term.fill_attr());

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
    csi_single_param_t param;
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
    csi_single_param_t param;
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

  static void do_el(term_t& term, line_t& line, csi_single_param_t param, attribute_t const& fill_attr) {
    board_t& b = term.board();
    tstate_t& s = term.state();
    if (param != 0 && param != 1) {
      if (!s.get_mode(mode_erm) && line.has_protected_cells()) {
        line_shift_flags flags = line_shift_flags::erm_protect;
        if (line.is_r2l(b.m_presentation_direction)) flags |= line_shift_flags::r2l;
        if (s.dcsm()) flags |= line_shift_flags::dcsm;
        line.shift_cells(0, b.m_width, b.m_width, flags, b.m_width, fill_attr);
      } else
        line.clear_content(b.m_width, fill_attr);
      return;
    }

    line_shift_flags flags = 0;
    if (!s.get_mode(mode_erm)) flags |= line_shift_flags::erm_protect;
    if (line.is_r2l(b.m_presentation_direction)) flags |= line_shift_flags::r2l;
    if (s.dcsm()) flags |= line_shift_flags::dcsm;

    if (s.get_mode(mode_xenl_ech)) b.cur.adjust_xenl();
    curpos_t x1 = b.cur.x();
    if (!s.dcsm()) x1 = b.to_presentation_position(b.cur.y(), x1);
    mwg_assert(x1 <= b.m_width);

    if (param == 0)
      line.shift_cells(x1, b.m_width, b.m_width - x1, flags, b.m_width, fill_attr);
    else
      line.shift_cells(0, x1 + 1, x1 + 1, flags, b.m_width, fill_attr);

    if (!s.dcsm()) x1 = b.to_data_position(b.cur.y(), x1);
    b.cur.update_x(x1);
  }

  bool do_el(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    do_el(term, term.board().line(), param, term.fill_attr());
    return true;
  }

  void do_ed(term_t& term, csi_single_param_t param) {
    tstate_t& s = term.state();
    board_t& b = term.board();
    attribute_t const fill_attr = term.fill_attr();
    curpos_t y1 = 0, y2 = 0;
    if (param != 0 && param != 1) {
      y1 = 0;
      y2 = b.m_height;
    } else {
      do_el(term, b.line(), param, fill_attr);
      if (param == 0) {
        y1 = b.cur.y() + 1;
        y2 = b.m_height;
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
        b.m_lines[y].clear_content(b.m_width, fill_attr);
    }
  }
  bool do_ed(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    do_ed(term, param);
    return true;
  }
  void do_decaln(term_t& term) {
    board_t& b = term.board();
    curpos_t const height = b.m_height;
    curpos_t const width = b.m_width;
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
    csi_single_param_t param;
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
    csi_single_param_t param;
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
  // Mouse report settings

  static void do_set_mouse_report(term_t& term, std::uint32_t spec) {
    tstate_t& s = term.state();
    s.mouse_mode = (s.mouse_mode & ~mouse_report_mask) | spec;
  }
  static void do_set_mouse_sequence(term_t& term, std::uint32_t spec) {
    tstate_t& s = term.state();
    s.mouse_mode = (s.mouse_mode & ~mouse_sequence_mask) | spec;
  }
  static int do_rqm_mouse_report(term_t& term, std::uint32_t spec) {
    tstate_t& s = term.state();
    return (s.mouse_mode & mouse_report_mask) == spec ? 1 : 2;
  }
  static int do_rqm_mouse_sequence(term_t& term, std::uint32_t spec) {
    tstate_t& s = term.state();
    return (s.mouse_mode & mouse_sequence_mask) == spec ? 1 : 2;
  }
  void do_sm_xtMouseX10(term_t& term, bool value)      { do_set_mouse_report(term, value ? mouse_report_down : 0); }
  void do_sm_xtMouseVt200(term_t& term, bool value)    { do_set_mouse_report(term, value ? mouse_report_xtMouseVt200 : 0); }
  void do_sm_xtMouseHilite(term_t& term, bool value)   { do_set_mouse_report(term, value ? mouse_report_xtMouseHilite : 0); }
  void do_sm_xtMouseButton(term_t& term, bool value)   { do_set_mouse_report(term, value ? mouse_report_xtMouseButton : 0); }
  void do_sm_xtMouseAll(term_t& term, bool value)      { do_set_mouse_report(term, value ? mouse_report_xtMouseAll : 0); }
  void do_sm_xtExtMouseUtf8(term_t& term, bool value)  { do_set_mouse_sequence(term, value ? mouse_sequence_utf8 : 0); }
  void do_sm_xtExtMouseSgr(term_t& term, bool value)   { do_set_mouse_sequence(term, value ? mouse_sequence_sgr : 0); }
  void do_sm_xtExtMouseUrxvt(term_t& term, bool value) { do_set_mouse_sequence(term, value ? mouse_sequence_urxvt : 0); }

  int do_rqm_xtMouseX10(term_t& term)      { return do_rqm_mouse_report(term, mouse_report_down); }
  int do_rqm_xtMouseVt200(term_t& term)    { return do_rqm_mouse_report(term, mouse_report_xtMouseVt200); }
  int do_rqm_xtMouseHilite(term_t& term)   { return do_rqm_mouse_report(term, mouse_report_xtMouseHilite); }
  int do_rqm_xtMouseButton(term_t& term)   { return do_rqm_mouse_report(term, mouse_report_xtMouseButton); }
  int do_rqm_xtMouseAll(term_t& term)      { return do_rqm_mouse_report(term, mouse_report_xtMouseAll); }
  int do_rqm_xtExtMouseUtf8(term_t& term)  { return do_rqm_mouse_sequence(term, mouse_sequence_utf8); }
  int do_rqm_xtExtMouseSgr(term_t& term)   { return do_rqm_mouse_sequence(term, mouse_sequence_sgr); }
  int do_rqm_xtExtMouseUrxvt(term_t& term) { return do_rqm_mouse_sequence(term, mouse_sequence_urxvt); }

  //---------------------------------------------------------------------------
  // device attributes

  void do_s7c1t(term_t& term) {
    term.state().set_mode(mode_s7c1t, true);
  }
  void do_s8c1t(term_t& term) {
    term.state().set_mode(mode_s7c1t, false);
  }
  bool do_decscl(term_t& term, csi_parameters& params) {
    csi_single_param_t param, s7c1t;
    params.read_param(param, 0);
    params.read_param(s7c1t, 0);
    if (param == 0) param = 65;

    term.state().m_decscl = param;
    term.state().set_mode(mode_s7c1t, param == 1);
    return true;
  }
  bool do_da1(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
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
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param != 0) return false;

    term.input_c1(ascii_csi);
    term.input_byte(ascii_greater);
    term.input_unsigned(67);
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
          if (term.board().cur.attribute.is_decsca_protected())
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
    // Note: 現在は暫定的にハードコーディングしているが、
    //   将来的にはunordered_map か何かで登録できる様にする。
    //   もしくは何らかの表からコードを自動生成する様にする。
    switch (modeSpec) {
    case mode_wystcurm1: // Mode 32 (Set Cursor Mode (Wyse))
      return !get_mode(mode_attCursorBlink) ? 1 : 2;
    case mode_wystcurm2: // Mode 33 WYSTCURM (Wyse Set Cursor Mode)
      return !get_mode(resource_cursorBlink) ? 1 : 2;
    case mode_wyulcurm: // Mode 34 WYULCURM (Wyse Underline Cursor Mode)
      return m_cursor_shape > 0 ? 1 : 2;
    case mode_altscreen: // Mode ?47
      return rqm_mode(mode_altscr);
    case mode_altscreen_clr: // Mode ?1047
      return rqm_mode(mode_altscr);
    case mode_decsc: // Mode ?1048
      return m_decsc_cur.x() >= 0 ? 1 : 2;
    case mode_altscreen_cur: // Mode ?1049
      return rqm_mode(mode_altscr);
    case mode_deccolm: return do_rqm_deccolm(*m_term);
    case mode_decscnm: return do_rqm_decscnm(*m_term);
    case mode_decawm : return do_rqm_decawm(*m_term);
    case mode_xtMouseX10     : return do_rqm_xtMouseX10(*m_term);
    case mode_xtMouseVt200   : return do_rqm_xtMouseVt200(*m_term);
    case mode_xtMouseHilite  : return do_rqm_xtMouseHilite(*m_term);
    case mode_xtMouseButton  : return do_rqm_xtMouseButton(*m_term);
    case mode_xtMouseAll     : return do_rqm_xtMouseAll(*m_term);
    case mode_xtExtMouseUtf8 : return do_rqm_xtExtMouseUtf8(*m_term);
    case mode_xtExtMouseSgr  : return do_rqm_xtExtMouseSgr(*m_term);
    case mode_xtExtMouseUrxvt: return do_rqm_xtExtMouseUrxvt(*m_term);
    default: return 0;
    }
  }

  void tstate_t::set_mode_with_accessor(mode_t modeSpec, bool value) {
    // Note: 現在は暫定的にハードコーディングしているが、
    //   将来的にはunordered_map か何かで登録できる様にする。
    //   もしくは何らかの表からコードを自動生成する様にする。
    switch (modeSpec) {
    case mode_wystcurm1:
      set_mode(mode_attCursorBlink, !value);
      break;
    case mode_wystcurm2:
      set_mode(resource_cursorBlink, !value);
      break;
    case mode_wyulcurm:
      if (value) {
        if (m_cursor_shape <= 0) m_cursor_shape = 1;
      } else {
        if (m_cursor_shape > 0) m_cursor_shape = 0;
      }
      break;
    case mode_altscreen:
      do_altscreen(*m_term, value);
      break;
    case mode_altscreen_clr:
      if (get_mode(mode_altscr) != value) {
        set_mode(mode_altscreen, value);
        if (value) do_ed(*m_term, 2);
      }
      break;
    case mode_decsc:
      if (value)
        do_decsc(*m_term);
      else
        do_decrc(*m_term);
      break;
    case mode_altscreen_cur:
      if (get_mode(mode_altscr) != value) {
        set_mode(mode_altscreen, value);
        set_mode(mode_decsc, value);
        if (value) do_ed(*m_term, 2);
      }
      break;
    case mode_deccolm: do_sm_deccolm(*m_term, value); break;
    case mode_decscnm: do_sm_decscnm(*m_term, value); break;
    case mode_decawm:  do_sm_decawm(*m_term, value);  break;
    case mode_xtMouseX10     : do_sm_xtMouseX10(*m_term, value);      break;
    case mode_xtMouseVt200   : do_sm_xtMouseVt200(*m_term, value);    break;
    case mode_xtMouseHilite  : do_sm_xtMouseHilite(*m_term, value);   break;
    case mode_xtMouseButton  : do_sm_xtMouseButton(*m_term, value);   break;
    case mode_xtMouseAll     : do_sm_xtMouseAll(*m_term, value);      break;
    case mode_xtExtMouseUtf8 : do_sm_xtExtMouseUtf8(*m_term, value);  break;
    case mode_xtExtMouseSgr  : do_sm_xtExtMouseSgr(*m_term, value);   break;
    case mode_xtExtMouseUrxvt: do_sm_xtExtMouseUrxvt(*m_term, value); break;
    default: ;
    }
  }


  struct control_function_dictionary {
    control_function_t* data1[63];
    std::unordered_map<std::uint16_t, control_function_t*> data2;

    void register_cfunc(control_function_t* fp, byte F) {
      int const index = (int) F - ascii_at;
      mwg_assert(0 <= index && index < (int) std::size(data1), "final byte out of range");
      mwg_assert(data1[index] == nullptr, "another function is already registered");
      data1[index] = fp;
    }

    void register_cfunc(control_function_t* fp, byte I, byte F) {
      data2[compose_bytes(I, F)] = fp;
    }

    control_function_dictionary() {
      std::fill(std::begin(data1), std::end(data1), nullptr);

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
    }

    control_function_t* get(byte F) const {
      int const index = (int) F - ascii_at;
      return 0 <= index && index < (int) std::size(data1) ? data1[index] : nullptr;
    }

    control_function_t* get(byte I, byte F) const {
      typedef std::unordered_map<std::uint16_t, control_function_t*>::const_iterator const_iterator;
      const_iterator const it = data2.find(compose_bytes(I, F));
      return it != data2.end() ? it->second : nullptr;
    }
  };

  static control_function_dictionary cfunc_dict;

  static bool process_private_control_sequence(term_t& term, sequence const& seq) {
    char32_t const* param = seq.parameter();
    std::size_t const len = seq.parameter_size();
    if (len > 0) {
      if (param[0] == '?') {
        // CSI ? ... Ft の形式
        csi_parameters params(param + 1, len - 1);
        if (!params) return false;

        if (seq.intermediate_size() == 0) {
          switch (seq.final()) {
          case ascii_h: return do_decset(term, params);
          case ascii_l: return do_decrst(term, params);
          }
        } else if (seq.intermediate_size() == 1) {
          if (seq.intermediate()[0] == ascii_dollar && seq.final() == ascii_y)
            return do_decrqm_dec(term, params);
        }
      } else if (param[0] == '>') {
        csi_parameters params(param + 1, len - 1);
        if (!params) return false;
        switch (seq.final()) {
        case ascii_c: return do_da2(term, params);
        }
      }
    }

    return false;
  }

  void term_t::process_control_sequence(sequence const& seq) {
    // check cursor state
#ifndef NDEBUG
    board_t& b = this->board();
    mwg_assert(b.cur.is_sane(b.m_width));
#endif

    if (seq.is_private_csi()) {
      if (!process_private_control_sequence(*this, seq))
        print_unrecognized_sequence(seq);

#ifndef NDEBUG
      mwg_assert(b.cur.is_sane(b.m_width),
        "cur: {x=%d, xenl=%d, width=%d} after CSI %c %c",
        b.cur.x(), b.cur.xenl(), b.m_width, seq.parameter()[0], seq.final());
#endif
      return;
    }

    auto& params = this->w_csiparams;
    params.initialize(seq);
    if (!params) {
      print_unrecognized_sequence(seq);
      return;
    }

    bool result = false;
    std::int32_t const intermediate_size = seq.intermediate_size();
    if (intermediate_size == 0) {
      if (seq.final() == ascii_m)
        result = do_sgr(*this, params);
      else if (control_function_t* const f = cfunc_dict.get(seq.final()))
        result = f(*this, params);
    } else if (intermediate_size == 1) {
      mwg_assert(seq.intermediate()[0] <= 0xFF);
      if (control_function_t* const f = cfunc_dict.get((byte) seq.intermediate()[0], seq.final()))
        result = f(*this, params);
    }
    mwg_assert(b.cur.is_sane(b.m_width),
      "cur: {x=%d, xenl=%d, width=%d} after CSI %c",
      b.cur.x(), b.cur.xenl(), b.m_width, seq.final());

    if (!result)
      print_unrecognized_sequence(seq);
  }

  void term_t::process_control_character(char32_t uchar) {
    switch (uchar) {
    case ascii_bel: do_bel(); break;
    case ascii_bs : do_bs(*this);  break;
    case ascii_ht : do_ht(*this);  break;
    case ascii_lf : do_lf(*this);  break;
    case ascii_ff : do_ff(*this);  break;
    case ascii_vt : do_vt(*this);  break;
    case ascii_cr : do_cr(*this);  break;

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

#ifndef NDEBUG
    board_t& b = board();
    mwg_assert(b.cur.is_sane(b.m_width),
      "cur: {x=%d, xenl=%d, width=%d} after C0/C1 %d",
      b.cur.x(), b.cur.xenl(), b.m_width, uchar);
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

  bool term_t::input_key(key_t key) {
    key_t mod = key & _modifier_mask;
    key_t code = key & _character_mask;

    // Meta は一律で ESC にする。
    if (mod & modifier_meta) {
      input_byte(ascii_esc);
      mod &= ~modifier_meta;
    }

    // テンキーの文字 (!DECNKM の時)
    if (!m_state.get_mode(ansi::mode_decnkm)) {
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
      default: ;
      }
    }

    // 通常文字
    if (mod == 0 && code < _key_base) {
      // Note: ESC, RET, HT はそのまま (C-[, C-m, C-i) 送信される。
      if (code == ascii_bs) code = ascii_del;
      contra::encoding::put_u8(code, input_buffer);
      input_flush();
      return true;
    }

    // C0 文字および DEL
    if (mod == modifier_control) {
      if (code == ascii_sp || code == ascii_a ||
        (ascii_a < code && code <= ascii_z) ||
        (ascii_left_bracket <= code && code <= ascii_underscore)
      ) {
        // C-@ ... C-_
        input_byte(code & 0x1F);
        input_flush();
        return true;
      } else if (code == ascii_question) {
        // C-? → ^? (DEL 0x7F)
        input_byte(ascii_del);
        input_flush();
        return true;
      } else if (code == ascii_bs) {
        // C-back → ^_ (US 0x1F)
        input_byte(ascii_us);
        input_flush();
        return true;
      }
    }

    // BS CR(RET) HT(TAB) ESC
    if (code < _key_base) {
      // CSI <Unicode> ; <Modifier> u 形式
      // input_c1(ascii_csi);
      // input_unsigned(code);
      // if (mod) {
      //   input_byte(ascii_semicolon);
      //   input_modifier(mod);
      // }
      // input_byte(ascii_u);

      // CSI 27 ; <Modifier> ; <Unicode> ~ 形式
      input_c1(ascii_csi);
      input_byte(ascii_2);
      input_byte(ascii_7);
      input_byte(ascii_semicolon);
      input_modifier(mod);
      input_byte(ascii_semicolon);
      input_unsigned(code);
      input_byte(ascii_tilde);
      input_flush();
      return true;
    }

    // application key mode の時修飾なしの関数キーは \eOA 等にする。
    char a = 0;
    switch (code) {
    case key_f1 : a = 11; goto tilde;
    case key_f2 : a = 12; goto tilde;
    case key_f3 : a = 13; goto tilde;
    case key_f4 : a = 14; goto tilde;
    case key_f5 : a = 15; goto tilde;
    case key_f6 : a = 17; goto tilde;
    case key_f7 : a = 18; goto tilde;
    case key_f8 : a = 19; goto tilde;
    case key_f9 : a = 20; goto tilde;
    case key_f10: a = 21; goto tilde;
    case key_f11: a = 23; goto tilde;
    case key_f12: a = 24; goto tilde;
    case key_f13: a = 25; goto tilde;
    case key_f14: a = 26; goto tilde;
    case key_f15: a = 28; goto tilde;
    case key_f16: a = 29; goto tilde;
    case key_f17: a = 31; goto tilde;
    case key_f18: a = 32; goto tilde;
    case key_f19: a = 33; goto tilde;
    case key_f20: a = 34; goto tilde;
    case key_f21: a = 36; goto tilde;
    case key_f22: a = 37; goto tilde;
    case key_f23: a = 38; goto tilde;
    case key_f24: a = 39; goto tilde;
    case key_home  : a = 1; goto tilde;
    case key_insert: a = 2; goto tilde;
    case key_delete: a = 3; goto tilde;
    case key_end   : a = 4; goto tilde;
    case key_prior : a = 5; goto tilde;
    case key_next  : a = 6; goto tilde;
    tilde:
      input_c1(ascii_csi);
      input_unsigned(a);
      if (mod) {
        input_byte(ascii_semicolon);
        input_modifier(mod);
      }
      input_byte(ascii_tilde);
      input_flush();
      return true;
    case key_up   : a = ascii_A; goto alpha;
    case key_down : a = ascii_B; goto alpha;
    case key_right: a = ascii_C; goto alpha;
    case key_left : a = ascii_D; goto alpha;
    case key_begin: a = ascii_E; goto alpha;
    alpha:
      if (mod) {
        input_c1(ascii_csi);
        input_byte(ascii_1);
        input_byte(ascii_semicolon);
        input_modifier(mod);
      } else {
        input_c1(m_state.get_mode(mode_decckm) ? ascii_ss3 : ascii_csi);
      }
      input_byte(a);
      input_flush();
      return true;
    case key_focus: a = ascii_I; goto alpha_focus;
    case key_blur:  a = ascii_O; goto alpha_focus;
    alpha_focus:
      if (m_state.get_mode(mode_xtSendFocus)) goto alpha;
      return false;
    default:
      return false;
    }
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
    bool const bracketed_paste_mode = state().get_mode(mode_bracketedPasteMode);
    if (bracketed_paste_mode) {
      input_c1(ascii_csi);
      input_unsigned(200);
      input_byte(ascii_tilde);
    }
    for (char32_t const u : data) input_uchar(u);
    if (bracketed_paste_mode) {
      input_c1(ascii_csi);
      input_unsigned(201);
      input_byte(ascii_tilde);
    }
    input_flush();
    return true;
  }

}
}
