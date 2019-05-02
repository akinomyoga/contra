// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_TERM_HPP
#define CONTRA_ANSI_TERM_HPP
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdarg>
#include <iterator>
#include <algorithm>
#include <vector>
#include "../sequence.hpp"
#include "line.hpp"
#include "../enc.c2w.hpp"
#include "../enc.utf8.hpp"

namespace contra {
namespace ansi {

  typedef std::uint32_t mode_t;

  enum mode_spec {
    ansi_mode   = 0, // CSI Ps h
    dec_mode    = 1, // CSI ? Ps h
    contra_mode = 2, // private mode

    accessor_flag = 0x10000,

#include "../../out/gen/term.mode_def.hpp"
  };

  struct tstate_t;
  class term_t;
  typedef std::uint32_t csi_single_param_t;

  void do_insert_graph(term_t& term, char32_t u);

  struct tstate_t {
    term_t* m_term;

    curpos_t page_home  {-1};
    curpos_t page_limit {-1};
    curpos_t line_home  {-1};
    curpos_t line_limit {-1};

    line_attr_t lflags {0};

    color_t m_rgba256[256];

    // OSC(0), scrTDS
    std::u32string title; // xterm title
    std::u32string screen_title; // GNU screen title

    // DECSC, SCOSC
    cursor_t m_decsc_cur;
    bool m_decsc_decawm;
    bool m_decsc_decom;
    curpos_t m_scosc_x;
    curpos_t m_scosc_y;
    bool m_scosc_xenl;

    // Alternate Screen Buffer
    board_t altscreen;

    // DECSTBM, DECSLRM
    curpos_t dec_tmargin = -1;
    curpos_t dec_bmargin = -1;
    curpos_t dec_lmargin = -1;
    curpos_t dec_rmargin = -1;

    // conformance level (61-65)
    int m_decscl = 65;

    // 画面サイズ
    curpos_t cfg_decscpp_enabled = true;
    curpos_t cfg_decscpp_min = 20;
    curpos_t cfg_decscpp_max = 400;

    // 既定の前景色・背景色
    byte m_default_fg_space = 0;
    byte m_default_bg_space = 0;
    color_t m_default_fg = 0;
    color_t m_default_bg = 0;

    tstate_t(term_t* term): m_term(term) {
      this->clear();
    }

    void clear() {
      this->initialize_mode();
      this->initialize_palette();

      this->title = U"";
      this->screen_title = U"";
      this->m_decsc_cur.set_x(-1);
      this->m_scosc_x = -1;

      this->page_home = -1;
      this->page_limit = -1;
      this->line_home = -1;
      this->line_limit = -1;
      this->clear_margin();

      this->m_decscl = 65;
    }
    void clear_margin() {
      this->dec_tmargin = -1;
      this->dec_bmargin = -1;
      this->dec_lmargin = -1;
      this->dec_rmargin = -1;
    }
    void initialize_palette() {
      color_t A = 0xFF000000;

      // rosaterm palette (rgba format)
      int index = 0;
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x80, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x80, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x00, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x80, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0xC0, 0xC0, 0xC0);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x80, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x32, 0xCD, 0x32);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0x7D, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0xFF);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0x00, 0xFF);
      m_rgba256[index++] = contra::dict::rgba(0x40, 0xE0, 0xD0);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0xFF, 0xFF);

      // 6x6x6 cube
      for (int r = 0; r < 6; r++) {
        color_t const R = A | r * 51;
        for (int g = 0; g < 6; g++) {
          color_t const RG = R | g * 51 << 8;
          for (int b = 0; b < 6; b++) {
            color_t const RGB = RG | b * 51 << 24;
            m_rgba256[index++] = RGB;
          }
        }
      }

      // 24 grayscale
      for (int k = 0; k < 24; k++) {
        color_t const K = k * 10 + 8;
        m_rgba256[index++] = A | K << 24 | K <<16 | K;
      }
    }

    sequence_decoder_config m_sequence_decoder_config;

    bool vt_affected_by_lnm    { true  };
    bool vt_appending_newline  { true  };
    bool vt_using_line_tabstop { false }; // ToDo: not yet supported

    bool ff_affected_by_lnm { true  };
    bool ff_clearing_screen { false };
    bool ff_using_page_home { false };

    /// @var cursor_shape
    ///   0 の時ブロックカーソル、1 の時下線、-1 の時縦線。
    ///   2 以上の時カーソルの下線の高さを百分率で表現。
    int m_cursor_shape;

  private:
    std::uint32_t m_mode_flags[2];

    void initialize_mode();

  private:
    bool get_mode_with_accessor(mode_t modeSpec) const;
  public:
    bool get_mode(mode_t modeSpec) const {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        unsigned const field = index >> 5;
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(field < sizeof(m_mode_flags) / sizeof(m_mode_flags[0]), "invalid modeSpec");;

        std::uint32_t const& flags = m_mode_flags[index >> 5];
        return (flags & bit) != 0;
      } else {
        return get_mode_with_accessor(modeSpec);
      }
    }

  private:
    void set_mode_with_accessor(mode_t modeSpec, bool value);
  public:
    void set_mode(mode_t modeSpec, bool value = true) {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        unsigned const field = index >> 5;
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(field < sizeof(m_mode_flags) / sizeof(m_mode_flags[0]), "invalid modeSpec");;

        std::uint32_t& flags = m_mode_flags[index >> 5];
        if (value)
          flags |= bit;
        else
          flags &= ~bit;
      } else {
        set_mode_with_accessor(modeSpec, value);
      }
    }

    bool dcsm() const { return get_mode(mode_bdsm) || get_mode(mode_dcsm); }

    bool is_cursor_visible() const { return get_mode(mode_dectcem); }
    bool is_cursor_blinking() const {
      bool const st = get_mode(mode_attCursorBlink);
      bool const mn = get_mode(resource_cursorBlink);
      return get_mode(resource_cursorBlinkXOR) ? st != mn : st || mn;
    }
    int get_cursor_shape() const { return m_cursor_shape; }

  public:
    void do_bel() {
      //std::fputc('\a', stdout);
    }

  public:
    std::size_t c2w(char32_t u) const {
      // ToDo: 文字幅設定
      return contra::encoding::c2w(u, contra::encoding::c2w_width_emacs);
    }
  };

  class term_t: public contra::idevice {
  private:
    board_t* m_board;
    tstate_t m_state {this};

    contra::idevice* m_send_target = nullptr;
    std::vector<byte> m_send_buff;

  public:
    void initialize_line(line_t& line) const {
      if (line.lflags() & line_attr_t::is_line_used) return;
      line.lflags() = line_attr_t::is_line_used | m_state.lflags;
      line.home() = m_state.line_home;
      line.limit() = m_state.line_limit;
    }

  public:
    term_t(contra::ansi::board_t& board): m_board(&board) {
      this->m_send_buff.reserve(32);
    }

    void set_input_target(contra::idevice& dev) { this->m_send_target = &dev; }

    void respond() {
      if (m_send_target)
        m_send_target->dev_write(reinterpret_cast<char const*>(&m_send_buff[0]), m_send_buff.size());
      m_send_buff.clear();
    }
    void response_put(byte value) {
      if (0x80 <= value && value < 0xA0 && m_state.get_mode(mode_s7c1t)) {
        m_send_buff.push_back(ascii_esc);
        m_send_buff.push_back(value - 0x40);
      } else {
        m_send_buff.push_back(value);
      }
    }
    void response_number(unsigned value) {
      if (value >= 10)
        response_number(value / 10);
      m_send_buff.push_back((byte) (ascii_0 + value % 10));
    }

  public:
    board_t& board() { return *m_board; }
    board_t const& board() const { return *m_board; }
    tstate_t& state() {return this->m_state;}
    tstate_t const& state() const {return this->m_state;}

    curpos_t tmargin() const {
      curpos_t const b = m_state.dec_tmargin;
      return 0 <= b && b < m_board->m_height ? b : 0;
    }
    curpos_t bmargin() const {
      curpos_t const e = m_state.dec_bmargin;
      return 0 < e && e <= m_board->m_height ? e : m_board->m_height;
    }
    curpos_t lmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return 0;
      curpos_t const l = m_state.dec_lmargin;
      return 0 <= l && l < m_board->m_width ? l : 0;
    }
    curpos_t rmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return m_board->m_width;
      curpos_t const r = m_state.dec_rmargin;
      return 0 < r && r <= m_board->m_width ? r : m_board->m_width;
    }

    curpos_t implicit_sph() const {
      curpos_t sph = 0;
      if (m_state.page_home > 0) sph = m_state.page_home;
      if (m_state.dec_tmargin > sph) sph = m_state.dec_tmargin;
      return sph;
    }
    curpos_t implicit_spl() const {
      curpos_t spl = m_board->m_height - 1;
      if (m_state.page_limit >= 0 && m_state.page_limit < spl)
        spl = m_state.page_limit;
      if (m_state.dec_bmargin > 0 && m_state.dec_bmargin - 1 < spl)
        spl = m_state.dec_bmargin - 1;
      return spl;
    }
    curpos_t implicit_slh(line_t const& line) const {
      curpos_t const width = m_board->m_width;
      curpos_t home = line.home();
      home = home < 0 ? 0 : std::min(home, width - 1);
      if (m_state.get_mode(mode_declrmm))
        home = std::max(home, std::min(m_state.dec_lmargin, width - 1));
      return home;
    }
    curpos_t implicit_sll(line_t const& line) const {
      curpos_t const width = m_board->m_width;
      curpos_t limit = line.limit();
      limit = limit < 0 ? width - 1 : std::min(limit, width - 1);
      if (m_state.get_mode(mode_declrmm) && m_state.dec_rmargin > 0)
        limit = std::min(limit, m_state.dec_rmargin - 1);
      return limit;
    }
    curpos_t implicit_sll() const {
      return implicit_sll(m_board->line(m_board->cur.y()));
    }
    curpos_t implicit_slh() const {
      return implicit_slh(m_board->line(m_board->cur.y()));
    }

    attribute_t fill_attr() const {
      if (m_state.get_mode(mode_bce)) {
        attribute_t const& src = m_board->cur.attribute;
        attribute_t ret;
        ret.set_bg(src.bg_space(), src.bg_color());
        return ret;
      } else {
        return {};
      }
    }

  public:
    void insert_marker(std::uint32_t marker) {
      bool const simd = m_state.get_mode(mode_simd);
      curpos_t const dir = simd ? -1 : 1;

      cell_t cell;
      cell.character = marker | character_t::flag_marker;
      cell.attribute = m_board->cur.attribute;
      cell.width = 0;
      initialize_line(m_board->line());
      m_board->line().write_cells(m_board->cur.x(), &cell, 1, 1, dir);
    }

    void insert_char(char32_t u) {
      u &= character_t::unicode_mask;

      // Unicode bidi formatting characters
      if ((U'\u202A' <= u && u <= U'\u202E') ||
        (U'\u2066' <= u && u <= U'\u2069'))
        return insert_marker(u);

      do_insert_graph(*this, u);

      board_t& b = board();
      mwg_assert(b.cur.is_sane(b.m_width),
        "cur: {x=%d, xenl=%d, width=%d} after Insert U+%04X",
        b.cur.x(), b.cur.xenl(), b.m_width, u);
    }

  public:
    void do_bel() {
      m_state.do_bel();
    }

  private:
    typedef sequence_decoder<term_t> decoder_type;
    decoder_type m_seqdecoder {this, &this->m_state.m_sequence_decoder_config};
    friend decoder_type;

    void print_unrecognized_sequence(sequence const& seq) {
      return; //取り敢えず off

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
      seq.print(stderr);
      std::fputc('\n', stderr);
    }
    void process_invalid_sequence(sequence const& seq) {
      // ToDo: 何処かにログ出力
      print_unrecognized_sequence(seq);
    }

    void process_escape_sequence(sequence const& seq);
    void process_control_sequence(sequence const& seq);
    void process_command_string(sequence const& seq);

    void process_character_string(sequence const& seq) {
      if (seq.type() == ascii_k) {
        m_state.screen_title = std::u32string(seq.parameter(), (std::size_t) seq.parameter_size());
        return;
      } else
        print_unrecognized_sequence(seq);
    }

    void process_control_character(char32_t uchar);

  public:
    std::uint64_t printt_state = 0;
    std::vector<char32_t> printt_buff;
    void write(const char* data, std::size_t const size) {
      printt_buff.resize(size);
      char32_t* const q0 = &printt_buff[0];
      char32_t* q1 = q0;
      contra::encoding::utf8_decode(data, data + size, q1, q0 + size, printt_state);
      for (char32_t const* q = q0; q < q1; q++)
        m_seqdecoder.process_char(*q);
    }
    void printt(const char* text) {
      write(text, std::strlen(text));
    }
    void putc(char32_t uchar) {
      m_seqdecoder.process_char(uchar);
    }

  private:
    virtual void dev_write(char const* data, std::size_t size) override {
      this->write(data, size);
    }
  };

}
}
#endif
