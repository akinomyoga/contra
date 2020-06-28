// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_sequence_hpp
#define contra_sequence_hpp
#include "contradef.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <unordered_map>
#include <mwg/except.h>
#include "enc.utf8.hpp"
#include "iso2022.hpp"

namespace contra {

  class sequence {
    byte                  m_type  {0};
    byte                  m_final {0};
    std::vector<char32_t> m_content;
    std::int32_t          m_intermediateStart {-1};

  public:
    void clear() {
      this->m_type = 0;
      this->m_final = 0;
      this->m_content.clear();
      this->m_intermediateStart = -1;
    }
    void append(char32_t uchar) {
      this->m_content.push_back(uchar);
    }
    void start_intermediate() {
      this->m_intermediateStart = this->m_content.size();
    }
    bool has_intermediate() const {
      return m_intermediateStart >= 0;
    }
    void set_type(byte type) {
      this->m_type = type;
    }
    void set_final(byte final) {
      this->m_final = final;
    }

  public:
    byte type() const {return m_type;}
    byte final() const {return m_final;}

    bool is_private_csi() const {
      return parameter_size() != 0 && ascii_less <= m_content[0] && m_content[0] <= ascii_question;
    }

    char32_t const* content() const {
      return &m_content[0];
    }
    std::int32_t content_size() const {
      return m_content.size();
    }

    char32_t const* parameter() const {
      return &m_content[0];
    }
    std::int32_t parameter_size() const {
      return m_intermediateStart < 0 ? m_content.size() : m_intermediateStart;
    }
    char32_t const* intermediate() const {
      return m_intermediateStart < 0 ? nullptr : &m_content[m_intermediateStart];
    }
    std::int32_t intermediate_size() const {
      return m_intermediateStart < 0 ? 0: m_content.size() - m_intermediateStart;
    }

  private:
    void print_char(std::FILE* file, char32_t ch) const {
      const char* name = get_ascii_name(ch);
      if (name)
        std::fprintf(file, "%s", name);
      else
        contra::encoding::put_u8(ch, file);
    }

  public:
    void print(std::FILE* file, int limit = -1) const {
      switch (m_type) {
      case ascii_csi:
      case ascii_esc:
        print_char(file, m_type);
        std::putc(' ', file);
        for (char32_t ch: m_content) {
          print_char(file, ch);
          std::putc(' ', file);
          if (--limit == 0) break;
        }
        print_char(file, m_final);
        break;
      default: // command string & characater string
        print_char(file, m_type);
        std::putc(' ', file);
        for (char32_t ch: m_content) {
          print_char(file, ch);
          std::putc(' ', file);
          if (--limit == 0) break;
        }
        std::fprintf(file, "ST");
        break;
      }
    }
  };

  struct sequence_decoder_config {
    bool c1_8bit_representation_enabled     = true; // 8bit C1 制御文字を有効にする。
    bool osc_sequence_terminated_by_bel     = true; // OSC シーケンスが BEL で終わっても良い。
    bool command_string_terminated_by_bel   = true; // 任意のコマンド列が BEL で終わっても良い。
    bool character_string_terminated_by_bel = true; // キャラクター列 (SOS, ESC k 等) が BEL で終わっても良い。
    bool title_definition_string_enabled    = true; // GNU Screen ESC k ... ST
  };

  template<typename Processor>
  class sequence_decoder {
    typedef Processor processor_type;

    enum decode_state {
      decode_default,
      decode_iso2022,
      decode_esc,
      decode_escape_sequence,
      decode_control_sequence,
      decode_command_string,
      decode_character_string,
    };

    processor_type* m_proc;
    sequence_decoder_config* m_config;
    decode_state m_dstate = decode_default;
    decode_state m_dstate_plain = decode_default;

    sequence m_seq;

    // command_string 及び character_string で使う状態
    bool m_hasPendingESC = false;

  public:
    sequence_decoder(Processor* proc, sequence_decoder_config* config): m_proc(proc), m_config(config) {}

  private:
    void clear() {
      this->m_seq.clear();
      this->m_dstate = m_dstate_plain;
    }

    void process_invalid_sequence() {
      m_proc->process_invalid_sequence(this->m_seq);
      clear();
    }

  private:
    // 以下の charset_t のメンバの値は、全て charset_t に加えて、
    // その文字集合が 94 であるか 96 であるかを識別する為のフラグを付加した物である。
    static constexpr charset_t _iso2022_96_iso8859_1 = iso2022_96_iso8859_1 | iso2022_charset_is96;
    charset_t m_iso2022_Gn[4] = { iso2022_94_iso646_usa, _iso2022_96_iso8859_1, 0, 0, };
    charset_t m_iso2022_GL = iso2022_94_iso646_usa;
    charset_t m_iso2022_GR = _iso2022_96_iso8859_1;
    int m_iso2022_GL_lock = 0;
    int m_iso2022_GR_lock = 1;
    charset_t m_iso2022_SS = iso2022_unspecified;

    // 複数バイト対応 (実質2Bにしか対応しない)
    int m_iso2022_mb_count = 0;
    charset_t m_iso2022_mb_charset = iso2022_unspecified;
    std::uint32_t m_iso2022_mb_buff = 0;
    bool m_iso2022_mb_right = false;

    void iso2022_update() {
      if (m_iso2022_GL == iso2022_94_iso646_usa &&
        m_iso2022_GR == _iso2022_96_iso8859_1 &&
        m_iso2022_SS == iso2022_unspecified
      ) {
        m_dstate_plain = decode_default;
      } else {
        m_dstate_plain = decode_iso2022;
      }
    }
    void iso2022_LSn(int n) {
      m_iso2022_GL_lock = n;
      m_iso2022_GL = m_iso2022_Gn[n];
      iso2022_update();
    }
    void iso2022_LSnR(int n) {
      m_iso2022_GR_lock = n;
      m_iso2022_GR = m_iso2022_Gn[n];
      iso2022_update();
    }
    void iso2022_GnD(int n, int charset) {
      m_iso2022_Gn[n] = charset;
      if (m_iso2022_GL_lock == n)
        m_iso2022_GL = charset;
      if (m_iso2022_GR_lock == n)
        m_iso2022_GR = charset;
      iso2022_update();
    }

    bool iso2022_escape_sequence() {
      if (this->m_seq.content_size() == 0) {
        switch (this->m_seq.final()) {
        case ascii_n: // LS2
          iso2022_LSn(2);
          return true;
        case ascii_o: // LS3
          iso2022_LSn(3);
          return true;
        case ascii_vertical_bar: // LS3R
          iso2022_LSnR(3);
          return true;
        case ascii_right_brace: // LS2R
          iso2022_LSnR(2);
          return true;
        case ascii_tilde: // LS1R
          iso2022_LSnR(1);
          return true;
        }
      } else {
        int seq_skip = 1;
        int n;
        iso2022_charset_size m;
        switch (this->m_seq.content()[0]) {
        case ascii_left_paren: // GZD4
          n = 0; m = iso2022_size_sb94;
          goto process_iso2022_GnDm;
        case ascii_right_paren: // G1D4
          n = 1; m = iso2022_size_sb94;
          goto process_iso2022_GnDm;
        case ascii_asterisk: // G2D4
          n = 2; m = iso2022_size_sb94;
          goto process_iso2022_GnDm;
        case ascii_plus: // G3D4
          n = 3; m = iso2022_size_sb94;
          goto process_iso2022_GnDm;
        case ascii_minus: // G1D6
          n = 1; m = iso2022_size_sb96;
          goto process_iso2022_GnDm;
        case ascii_dot: // G2D6
          n = 2; m = iso2022_size_sb96;
          goto process_iso2022_GnDm;
        case ascii_slash: // G3D6
          n = 3; m = iso2022_size_sb96;
          goto process_iso2022_GnDm;
        case ascii_dollar:
          if (this->m_seq.content_size() == 1) {
            // ESC $ @, ESC $ A, ESC $ B は特別扱い
            if (ascii_at <= m_seq.final() && m_seq.final() <= ascii_B) {
              n = 0; m = iso2022_size_mb94;
              goto process_iso2022_GnDm;
            }
          } else {
            seq_skip = 2;
            switch (m_seq.content()[1]) {
            case ascii_left_paren: // GZDM4
              n = 0; m = iso2022_size_mb94;
              goto process_iso2022_GnDm;
            case ascii_right_paren: // G1DM4
              n = 1; m = iso2022_size_mb94;
              goto process_iso2022_GnDm;
            case ascii_asterisk: // G2DM4
              n = 2; m = iso2022_size_mb94;
              goto process_iso2022_GnDm;
            case ascii_plus: // G3DM4
              n = 3; m = iso2022_size_mb94;
              goto process_iso2022_GnDm;
            case ascii_minus: // G1DM6
              n = 1; m = iso2022_size_mb96;
              goto process_iso2022_GnDm;
            case ascii_dot: // G2DM6
              n = 2; m = iso2022_size_mb96;
              goto process_iso2022_GnDm;
            case ascii_slash: // G3DM6
              n = 3; m = iso2022_size_mb96;
              goto process_iso2022_GnDm;
            case ascii_sp: // [T.61/E.3.1.2] DRCS の時は ( を省略しても良い?
              seq_skip = 1;
              n = 0; m = iso2022_size_mb94;
              goto process_iso2022_GnDm;
            }
          }
          break;
        process_iso2022_GnDm:
          {
            // ESC(B は terminfo に登録されて頻繁に呼び出される。
            // 初期化を遅延する為に特別に処理する事にする。
            if (seq_skip == m_seq.content_size()) {
              if (m == iso2022_size_sb94 && m_seq.final() == ascii_B) {
                iso2022_GnD(n, iso2022_94_iso646_usa);
                return true;
              } else if (m == iso2022_size_sb96 && m_seq.final() == ascii_A) {
                iso2022_GnD(n, _iso2022_96_iso8859_1);
                return true;
              }
            }

            auto& iso2022 = iso2022_charset_registry::instance();
            char32_t const* intermediate = m_seq.content() + seq_skip;
            std::size_t const intermediate_size = m_seq.content_size() - seq_skip;
            charset_t const charset = iso2022.resolve_designator(m, intermediate, intermediate_size, m_seq.final());
            if (charset != iso2022_unspecified) {
              iso2022_GnD(n, charset);
              return true;
            } else {
              // unrecognized charset
              // ■ログ機能を実装したら詳細な情報をそちらに出力する
            }
          }
          break;
        }
      }
      return false;
    }

    void iso2022_send_char(charset_t charset, std::uint32_t char_index) {
      charset_t const cssize = charset & charflag_iso2022_db ? 96 * 96 : 96;
      std::uint32_t const charset_index = charset & charflag_iso2022_mask_code;
      std::uint32_t const flags = (charset & charflag_iso2022_mask_flag) | charflag_iso2022;
      m_proc->process_char((charset_index * cssize + char_index) | flags);
    }
    void iso2022_mb_start(charset_t charset, char32_t uchar) {
      m_iso2022_mb_count = 1; // 今は2バイト文字集合のみ対応
      m_iso2022_mb_charset = charset;
      m_iso2022_mb_buff = (uchar & 0x7F) - 0x20;
      m_iso2022_mb_right = uchar >= 0x80;
    }
    bool iso2022_mb_add(char32_t uchar) {
      // GL/GR が混ざった multibyte は駄目
      if (m_iso2022_mb_right != (uchar >= 0x80)) return false;

      // 94/94n文字集合の時 2/0 は駄目。7/15 は無視する。
      if (!(m_iso2022_mb_charset & iso2022_charset_is96)) {
        if (uchar == ascii_sp) return false;
        if (uchar == ascii_del) return true;
      }

      m_iso2022_mb_buff = m_iso2022_mb_buff * 96 + ((uchar & 0x7F) - 0x20);
      if (--m_iso2022_mb_count == 0) {
        // ISO-2022 複数バイト文字確定
        m_iso2022_SS = iso2022_unspecified;
        iso2022_send_char(m_iso2022_mb_charset, m_iso2022_mb_buff);
      }
      return true;
    }

    void process_char_iso2022(char32_t uchar) {
      if (uchar < 0x100 && 0x20 <= (uchar & 0x7F)) {
        // GL/GR の文字

        // 複数バイト文字集合の読み取り途中
        if (m_iso2022_mb_count) {
          if (iso2022_mb_add(uchar)) return;

          // 復号エラーの時は 0xFFFD を出力して、
          // 状態をリセットして普通に処理する。
          m_proc->process_char(0xFFFD);
          m_iso2022_mb_count = 0;
          m_iso2022_SS = iso2022_unspecified;
        }

        charset_t charset;
        if (m_iso2022_SS != iso2022_unspecified) {
          charset = m_iso2022_SS;

          // 現在の実装では単一シフト領域を GL/GR のどちらにも規定しない。
          // 送信側がどちらを想定していても動く様にする。

          // 94/94n 文字集合を SS した直後の 2/0 及び 7/15 は駄目
          if (!(charset & iso2022_charset_is96) && (uchar == ascii_sp || uchar == ascii_del)) {
            // DEL は無視する (CC-data-element から除いてなかった事にする)
            if (uchar == ascii_del) return;

            m_proc->process_char(0xFFFD);
            m_iso2022_SS = iso2022_unspecified;
            process_char_iso2022(uchar); // retry
            return;
          }
        } else {
          charset = uchar < 0x80 ? m_iso2022_GL : m_iso2022_GR;
          if (!(charset & iso2022_charset_is96) && (uchar == ascii_sp || uchar == ascii_del)) {
            // DEL は無視する (CC-data-element から除いてなかった事にする)
            if (uchar == ascii_del) return;

            m_proc->process_char(uchar);
            return;
          }
        }

        if (charset & charflag_iso2022_db) {
          iso2022_mb_start(charset, uchar);
        } else {
          m_iso2022_SS = iso2022_unspecified;
          if (charset == iso2022_94_iso646_usa)
            m_proc->process_char(uchar & 0x7F);
          else if (charset == _iso2022_96_iso8859_1)
            m_proc->process_char((uchar & 0x7F) | 0x80);
          else
            iso2022_send_char(charset, (uchar & 0x7F) - 0x20);
        }
      } else {
        if (m_iso2022_mb_count) {
          m_proc->process_char(0xFFFD);
          m_iso2022_mb_count = 0;
          m_iso2022_SS = iso2022_unspecified;
        } else if (m_iso2022_SS != iso2022_unspecified) {
          m_proc->process_char(0xFFFD);
          m_iso2022_SS = iso2022_unspecified;
        }
        process_char_default(uchar);
      }
    }

  private:
    void process_c0(char32_t uchar) {
      switch (uchar) {
      case ascii_so: // SO/LS1
        iso2022_LSn(1);
        return;
      case ascii_si: // SI/LS0
        iso2022_LSn(0);
        return;
      }
      m_proc->process_control_character(uchar);
    }
    void process_escape_sequence() {
      if (!iso2022_escape_sequence())
        m_proc->process_escape_sequence(this->m_seq);
      clear();
    }

    void process_control_sequence() {
      m_proc->process_control_sequence(this->m_seq);
      clear();
    }

    void process_command_string() {
      m_proc->process_command_string(this->m_seq);
      clear();
      this->m_hasPendingESC = false;
    }

    void process_character_string() {
      m_proc->process_character_string(this->m_seq);
      clear();
      this->m_hasPendingESC = false;
    }

    void process_char_for_escape_sequence(char32_t uchar) {
      if (0x30 <= uchar && uchar <= 0x7E) {
        m_seq.set_final((byte) uchar);
        process_escape_sequence();
      } else if (0x20 <= uchar && uchar <= 0x2F) {
        m_seq.append(uchar);
      } else {
        process_invalid_sequence();
        decode_char(uchar);
      }
    }

    void process_char_for_control_sequence(char32_t uchar) {
      if (0x40 <= uchar && uchar <= 0x7E) {
        m_seq.set_final((byte) uchar);
        process_control_sequence();
      } else {
        if (m_seq.has_intermediate()) {
          if (0x20 <= uchar && uchar <= 0x2F) {
            m_seq.append(uchar);
            return;
          }
        } else {
          if (0x20 <= uchar && uchar <= 0x3F) {
            if (uchar <= 0x2F)
              m_seq.start_intermediate();
            m_seq.append(uchar);
            return;
          }
        }

        // Note: 事情はよく分からないが vttest は HT, VT, CR をその場で処理する事を要求する。
        //   RLogin はエスケープシーケンス以外の C0 を全てその場で処理している気がする。
        //   SI, SO に関しても文字コード制御の効果を持っている。
        if (0 <= uchar && uchar < 0x20 && uchar != ascii_esc) {
          process_c0(uchar);
          return;
        }

        m_seq.set_final(uchar);
        process_invalid_sequence();
        decode_char(uchar);
      }
    }

    bool is_command_string_terminator(char32_t uchar) const {
      if (uchar == ascii_st)
        return m_config->c1_8bit_representation_enabled;
      else if (uchar == ascii_bel) {
        if (m_config->command_string_terminated_by_bel) return true;
        if (m_seq.type() == ascii_osc) return m_config->osc_sequence_terminated_by_bel;
        return false;
      } else
        return false;
    }

    void process_char_for_command_string(char32_t uchar) {
      if (m_hasPendingESC) {
        if (uchar == ascii_backslash) {
          process_command_string();
        } else {
          process_invalid_sequence();
          decode_char(ascii_esc);
          decode_char(uchar);
        }

        return;
      }

      if ((0x08 <= uchar && uchar <= 0x0D) || (0x20 <= uchar && uchar <= 0x7E)) {
        m_seq.append(uchar);
      } else if (uchar == ascii_esc) {
        m_hasPendingESC = true;
      } else if (is_command_string_terminator(uchar)) {
        process_command_string();
      } else {
        process_invalid_sequence();
        decode_char(uchar);
      }
    }

    bool is_character_string_terminator(char32_t uchar) const {
      if (uchar == ascii_st)
        return m_config->c1_8bit_representation_enabled;
      else if (uchar == ascii_bel)
        return m_config->character_string_terminated_by_bel;
      else
        return false;
    }

    void process_char_for_character_string(char32_t uchar) {
      if (m_hasPendingESC) {
        if (uchar == ascii_backslash || is_character_string_terminator(uchar)) {
          // final ST
          if (uchar == ascii_st)
            m_seq.append(ascii_esc);
          process_character_string();
          return;
        } else if (m_seq.type() == ascii_sos &&
          (uchar == ascii_X || (uchar == ascii_sos && m_config->c1_8bit_representation_enabled))) {
          // invalid SOS
          process_invalid_sequence();
          if (uchar == ascii_X)
            decode_char(ascii_esc);
          decode_char(uchar);
          return;
        } else if (uchar == ascii_esc) {
          m_seq.append(ascii_esc);
          m_hasPendingESC = false;
        }
      }

      if (uchar == ascii_esc) {
        m_hasPendingESC = true;
      } else if (is_character_string_terminator(uchar)) {
        process_character_string();
      } else if (uchar == ascii_sos && m_config->c1_8bit_representation_enabled) {
        process_invalid_sequence();
        decode_char(uchar);
      } else
        m_seq.append(uchar);
    }

    void process_char_default_c1(char32_t c1char) {
      switch (c1char) {
      case ascii_csi:
        m_seq.set_type((byte) c1char);
        m_dstate = decode_control_sequence;
        break;
      case ascii_sos:
        m_seq.set_type((byte) c1char);
        m_dstate = decode_character_string;
        break;
      case ascii_dcs:
      case ascii_osc:
      case ascii_pm:
      case ascii_apc:
        m_seq.set_type((byte) c1char);
        m_dstate = decode_command_string;
        break;
      default:
        m_proc->process_control_character(c1char);
        break;
      }
    }

    void process_esc_char(char32_t uchar) {
      m_dstate = m_dstate_plain;
      if (0x20 <= uchar && uchar < 0x7E) {
        if (uchar < 0x30) {
          // <I> to start an escape sequence
          m_seq.set_type((byte) ascii_esc);
          m_seq.append((byte) uchar);
          m_dstate = decode_escape_sequence;
        } else if (0x40 <= uchar && uchar < 0x60) {
          // <F> to call C1
          process_char_default_c1((uchar & 0x1F) | 0x80);
        } else if (uchar == ascii_k && m_config->title_definition_string_enabled) {
          m_seq.set_type((byte) ascii_k);
          m_dstate = decode_character_string;
        } else {
          // <F>/<P> to complete an escape sequence
          m_seq.set_type((byte) ascii_esc);
          m_seq.set_final((byte) uchar);
          process_escape_sequence();
        }
      } else {
        process_invalid_sequence();
      }
    }

  private:
    void process_char_default(char32_t uchar) {
      if (0 <= uchar && uchar < 0x20) {
        if (uchar == ascii_esc)
          m_dstate = decode_esc;
        else {
          process_c0(uchar);
          m_dstate = m_dstate_plain;
        }
      } else if (0x80 <= uchar && uchar < 0xA0) {
        if (m_config->c1_8bit_representation_enabled)
          process_char_default_c1(uchar);
      } else {
        m_proc->process_char(uchar);
      }
    }

  public:
    void decode_char(char32_t uchar) {
      if ((uchar == ascii_nul || uchar == ascii_del) &&
        m_dstate != decode_iso2022) return;

      switch (m_dstate) {
      case decode_default:
        process_char_default(uchar);
        break;
      case decode_iso2022:
        process_char_iso2022(uchar);
        break;
      case decode_esc:
        process_esc_char(uchar);
        break;
      case decode_escape_sequence:
        process_char_for_escape_sequence(uchar);
        break;
      case decode_control_sequence:
        process_char_for_control_sequence(uchar);
        break;
      case decode_command_string:
        process_char_for_command_string(uchar);
        break;
      case decode_character_string:
        process_char_for_character_string(uchar);
        break;
      }
    }
    static constexpr bool is_ctrl(std::uint32_t uchar) {
      std::uint32_t u = uchar & ~(std::uint32_t) 0x80;
      return u < 0x20 || uchar == ascii_del;
    }
    void decode(char32_t const* beg, char32_t const* end) {
      while (beg != end) {
        if (m_dstate == decode_default) {
          char32_t const uchar = *beg;
          if (!is_ctrl(uchar)) {
            // 通常文字の連続
            char32_t const* const batch_begin = beg;
            do beg++; while (beg != end && !is_ctrl(*beg));
            char32_t const* const batch_end = beg;
            m_proc->process_chars(batch_begin, batch_end);
          } else {
            if (uchar < 0x20 && (0xF7FF3FFEu & 1 << uchar)) {
              // 速度のため頻出制御文字は先に分岐
              // Note: 0xF7FF3FFEu は sequence_decoder で特別に
              //   処理する必要のある NUL, SO, SI, ESC を除いた物。
              m_proc->process_control_character(uchar);
            } else if (uchar != ascii_nul && uchar != ascii_del) {
              process_char_default(uchar);
            }
            beg++;
          }
        } else
          decode_char(*beg++);
      }
    }

    void process_end() {
      switch (m_dstate) {
      case decode_escape_sequence:
      case decode_control_sequence:
      case decode_command_string:
      case decode_character_string:
        process_invalid_sequence();
        break;
      default: break;
      }
    }
  };

  class sequence_printer: public idevice {
    const char* fname;
    std::FILE* const file;
  public:
    sequence_printer(std::FILE* file):
      fname(nullptr), file(file) {}
    sequence_printer(const char* fname):
      fname(fname), file(std::fopen(fname, "w")) {}
    ~sequence_printer() {if (fname) std::fclose(this->file);}

  private:
    sequence_decoder_config m_config; // ToDo: should be simpler instance

    typedef sequence_decoder<sequence_printer> decoder_type;
    decoder_type m_seqdecoder {this, &this->m_config};
    friend decoder_type;

    bool m_hasPrecedingChar {false};

    void print_sequence(sequence const& seq) {
      if (m_hasPrecedingChar) {
        m_hasPrecedingChar = false;
        std::fputc('\n', file);
      }

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
      case ascii_k: // screen TITLE DEFINITION STRING
        name = "private string";
        break;
      }

      std::fprintf(file, "%s: ", name);
      seq.print(file);
      std::fputc('\n', file);
      std::fflush(file);
    }

    void process_invalid_sequence(sequence const& seq) {
      // ToDo: 何処かにログ出力
      print_sequence(seq);
    }

    void process_escape_sequence(sequence const& seq) {
      print_sequence(seq);
    }

    void process_control_sequence(sequence const& seq) {
      print_sequence(seq);
    }

    void process_command_string(sequence const& seq) {
      print_sequence(seq);
    }
    void process_character_string(sequence const& seq) {
      print_sequence(seq);
    }

    void process_char(char32_t u) {
      if (m_hasPrecedingChar) {
        std::fputc(' ', file);
      } else
        m_hasPrecedingChar = true;

      if (u > 0xFF)
        std::fprintf(file, "U+%04X", (unsigned) u);
      else if (u == U' ')
        std::fprintf(file, "SP");
      else
        std::fprintf(file, "%c", (char) u);
    }
    void process_chars(char32_t const* beg, char32_t const* end) {
      while (beg < end) process_char(*beg++);
    }

    void process_control_character(char32_t uchar) {
      if (m_hasPrecedingChar) {
        std::fputc(' ', file);
      } else
        m_hasPrecedingChar = true;

      const char* name = get_ascii_name(uchar);
      if (name)
        std::fprintf(file, "%s", name);
      else
        std::fprintf(file, "%c", uchar);
    }

  public:
    void write(const char* data, std::size_t size) {
      for (std::size_t i = 0; i < size; i++)
        m_seqdecoder.decode_char(data[i]);
    }

  private:
    virtual void dev_write(char const* data, std::size_t size) override {
      this->write(data, size);
    }
  };

  //---------------------------------------------------------------------------
  // CSI parameters

  typedef std::uint32_t csi_single_param_t;

  class csi_parameters;

  struct csi_param_type {
    csi_single_param_t value;
    bool isColon;
    bool isDefault;
  };

  class csi_parameters {
    std::vector<csi_param_type> m_data;
    std::size_t m_index {0};
    bool m_result;

  public:
    csi_parameters(char32_t const* s, std::size_t n) {
      initialize();
      read_parameters(s, n);
    }
    csi_parameters(sequence const& seq):
      csi_parameters(seq.parameter(), seq.parameter_size()) {}

    csi_parameters(): m_result(false) {}

    void initialize() {
      m_data.clear();
      m_index = 0;
      m_isColonAppeared = false;
      m_result = false;
    }

    void initialize(sequence const& seq) {
      initialize();
      read_parameters(seq.parameter(), seq.parameter_size());
    }

    operator bool() const {return this->m_result;}

  public:
    std::size_t size() const {return m_data.size();}
    void push_back(csi_param_type const& value) {m_data.push_back(value);}

  private:
    bool read_parameters(char32_t const* str, std::size_t len) {
      bool isSet = false;
      csi_param_type param {0, false, true};
      for (std::size_t i = 0; i < len; i++) {
        char32_t const c = str[i];
        if (!(ascii_0 <= c && c <= ascii_semicolon)) {
          std::fprintf(stderr, "invalid value of CSI parameter values.\n");
          return false;
        }

        if (c <= ascii_9) {
          std::uint32_t const newValue = param.value * 10 + (c - ascii_0);
          if (newValue < param.value) {
            // overflow
            std::fprintf(stderr, "a CSI parameter value is too large.\n");
            return false;
          }

          isSet = true;
          param.value = newValue;
          param.isDefault = false;
        } else {
          m_data.push_back(param);
          isSet = true;
          param.value = 0;
          param.isColon = c == ascii_colon;
          param.isDefault = true;
        }
      }

      if (isSet) m_data.push_back(param);
      return m_result = true;
    }

  public:
    bool m_isColonAppeared;

    bool read_param(csi_single_param_t& result, std::uint32_t defaultValue) {
      while (m_index < m_data.size()) {
        csi_param_type const& param = m_data[m_index++];
        if (!param.isColon) {
          m_isColonAppeared = false;
          if (!param.isDefault)
            result = param.value;
          else
            result = defaultValue;
          return true;
        }
      }
      result = defaultValue;
      return false;
    }

    bool read_arg(csi_single_param_t& result, bool toAllowSemicolon, csi_single_param_t defaultValue) {
      if (m_index < m_data.size()
        && (m_data[m_index].isColon || (toAllowSemicolon && !m_isColonAppeared))
      ) {
        csi_param_type const& param = m_data[m_index++];
        if (param.isColon) m_isColonAppeared = true;
        if (!param.isDefault)
          result = param.value;
        else
          result = defaultValue;
        return true;
      }

      result = defaultValue;
      return false;
    }

    void debug_print(std::FILE* file) const {
      bool is_first = true;
      for (auto const& entry : m_data) {
        if (entry.isColon)
          std::putc(':', file);
        else if (!is_first)
          std::putc(';', file);
        if (!entry.isDefault)
          std::fprintf(file, "%u", entry.value);
        is_first = false;
      }
    }
  };

  //---------------------------------------------------------------------------
  // input_decoder

  enum input_data_type {
    input_data_paste = 1,
  };

  template<typename Processor>
  class input_decoder {
    Processor* m_proc;

    // requires the following members:
    //   m_proc->process_key(u);
    //   m_proc->process_input_data(type, data);
    //   m_proc->process_invalid_sequence(seq);
    //   m_proc->process_control_sequence(seq);
    //
    // ToDo: use TRIE?
    // ToDo: use terminfo?

  public:
    input_decoder(Processor* proc): m_proc(proc) {}

  private:
    enum decode_state {
      decode_default = 0,
      decode_esc,
      decode_csi,
      decode_paste_default,
      decode_paste_esc,
      decode_paste_csi,
      decode_paste_csi2,
      decode_paste_csi20,
      decode_paste_csi201,
    };

    int m_state = 0;
    sequence m_seq;
    bool has_pending_esc = false;
    int m_paste_suffix = 0;
    std::u32string m_paste_data;
  public:
    void decode(char32_t const* beg, char32_t const* end) {
      while (beg != end) decode_char(*beg++);
    }
    void decode_char(char32_t u) {
      switch (m_state) {
      case decode_default:
        if (u == ascii_esc) {
          m_state = decode_esc;
        } else if (u == ascii_csi) {
          m_state = decode_csi;
          m_seq.set_type(ascii_csi);
        } else {
          process_key(u & _character_mask);
        }
        break;
      case decode_esc:
        if (u == ascii_left_bracket) {
          m_state = decode_csi;
          m_seq.set_type(ascii_csi);
        } else {
          process_key(ascii_esc);
          m_state = decode_default;
          decode_char(u);
        }
        break;
      case decode_csi:
        if (0x40 <= u && u <= 0x7E) {
          m_seq.set_final((byte) u);
          process_control_sequence();
          m_state = decode_default;
          m_seq.clear();
        } else if (0x20 <= u && u <= 0x2F) {
          if (!m_seq.has_intermediate())
            m_seq.start_intermediate();
          m_seq.append(u);
        } else if (0x30 <= u  && u <= 0x3F) {
          if (m_seq.has_intermediate()) {
            m_seq.set_final((byte) u);
            process_invalid_sequence();
            m_state = decode_default;
            m_seq.clear();
            decode_char(u);
          } else {
            m_seq.append(u);
          }
        } else {
          m_seq.set_final((byte) u);
          process_invalid_sequence();
          m_state = decode_default;
          m_seq.clear();
          decode_char(u);
        }
        break;
      case decode_paste_default:
        m_paste_data += u;
        if (u == ascii_esc) {
          m_state = decode_paste_esc;
          m_paste_suffix = 1;
        } else if (u == ascii_csi) {
          m_state = decode_paste_csi;
          m_paste_suffix = 1;
        }
        break;
      case decode_paste_esc:
        m_paste_data += u;
        m_state = u == ascii_left_bracket ? decode_paste_csi : decode_paste_default;
        m_paste_suffix++;
        break;
      case decode_paste_csi:
        m_paste_data += u;
        m_state = u == ascii_2 ? decode_paste_csi2 : decode_paste_default;
        m_paste_suffix++;
        break;
      case decode_paste_csi2:
        m_paste_data += u;
        m_state = u == ascii_0 ? decode_paste_csi20 : decode_paste_default;
        m_paste_suffix++;
        break;
      case decode_paste_csi20:
        m_paste_data += u;
        m_state = u == ascii_1 ? decode_paste_csi201 : decode_paste_default;
        m_paste_suffix++;
        break;
      case decode_paste_csi201:
        m_paste_data += u;
        m_state = u == ascii_1 ? decode_paste_csi201 : decode_paste_default;
        m_paste_suffix++;
        if (u == ascii_tilde) {
          m_paste_data.erase(m_paste_data.end() - m_paste_suffix, m_paste_data.end());
          process_input_data(input_data_paste, m_paste_data);
        }
        break;
      default:
        mwg_assert(false, "BUG unexpected");
        m_state = decode_default;
        process_key(u & _character_mask);
        break;
      }
    }

  private:
    void process_key(std::uint32_t u) {
      if (u == ascii_esc && !has_pending_esc) {
        has_pending_esc = true;
        return;
      }

      if (u < 32) {
        if (1 <= u && u <= 26)
          u |= 0x60 | modifier_control;
        else
          u |= 0x40 | modifier_control;
      }

      if (has_pending_esc) {
        has_pending_esc = false;
        if (u & modifier_meta)
          process_key(ascii_esc);
        else
          u |= modifier_meta;
      }

      m_proc->process_key(u);
    }

    void process_input_data(input_data_type type, std::u32string const& data) {
      m_proc->process_input_data(type, data);
    }

    void process_invalid_sequence() {
      m_proc->process_invalid_sequence(m_seq);
    }

    void process_control_sequence() {
      if (!process_csi_key())
        m_proc->process_control_sequence(m_seq);
    }

    bool process_csi_key() {
      csi_parameters params(m_seq);
      if (!params) return false;

      key_t key = 0;
      switch (m_seq.final()) {
      case ascii_tilde:
        {
          csi_single_param_t param1;
          params.read_param(param1, 0);

          switch (param1) {
          case 1: key = key_home; goto modified_key;
          case 2: key = key_insert; goto modified_key;
          case 3: key = key_delete; goto modified_key;
          case 4: key = key_end; goto modified_key;
          case 5: key = key_prior; goto modified_key;
          case 6: key = key_next; goto modified_key;
          case 7: key = key_home; goto modified_key;
          case 8: key = key_end; goto modified_key;
          case 11: key = key_f1; goto modified_key;
          case 12: key = key_f2; goto modified_key;
          case 13: key = key_f3; goto modified_key;
          case 14: key = key_f4; goto modified_key;
          case 15: key = key_f5; goto modified_key;
          case 17: key = key_f6; goto modified_key;
          case 18: key = key_f7; goto modified_key;
          case 19: key = key_f8; goto modified_key;
          case 20: key = key_f9; goto modified_key;
          case 21: key = key_f10; goto modified_key;
          case 23: key = key_f11; goto modified_key;
          case 24: key = key_f12; goto modified_key;
          case 25: key = key_f13; goto modified_key;
          case 26: key = key_f14; goto modified_key;
          case 28: key = key_f15; goto modified_key;
          case 29: key = key_f16; goto modified_key;
          case 31: key = key_f17; goto modified_key;
          case 32: key = key_f18; goto modified_key;
          case 33: key = key_f19; goto modified_key;
          case 34: key = key_f20; goto modified_key;

          case 27: goto modified_code;
          case 200: goto paste_begin; // paste_begin
            // case 201: // paste_end
          }
        }
        break;
      case ascii_A: key = key_up; goto modified_alpha;
      case ascii_B: key = key_down; goto modified_alpha;
      case ascii_C: key = key_right; goto modified_alpha;
      case ascii_D: key = key_left; goto modified_alpha;
      case ascii_E: key = key_begin; goto modified_alpha;
      case ascii_F: key = key_end; goto modified_alpha;
      case ascii_H: key = key_home; goto modified_alpha;
      case ascii_M: key = key_kpent; goto modified_alpha;
      case ascii_P: key = key_kpf1; goto modified_alpha;
      case ascii_Q: key = key_kpf2; goto modified_alpha;
      case ascii_R: key = key_kpf3; goto modified_alpha;
      case ascii_S: key = key_kpf4; goto modified_alpha;
      case ascii_j: key = key_kpmul; goto modified_alpha;
      case ascii_k: key = key_kpadd; goto modified_alpha;
      case ascii_l: key = key_kpsep; goto modified_alpha;
      case ascii_m: key = key_kpsub; goto modified_alpha;
      case ascii_n: key = key_kpdec; goto modified_alpha;
      case ascii_o: key = key_kpdiv; goto modified_alpha;
      case ascii_p: key = key_kp0; goto modified_alpha;
      case ascii_q: key = key_kp1; goto modified_alpha;
      case ascii_r: key = key_kp2; goto modified_alpha;
      case ascii_s: key = key_kp3; goto modified_alpha;
      case ascii_t: key = key_kp4; goto modified_alpha;
      case ascii_u: key = key_kp5; goto modified_unicode;
      case ascii_v: key = key_kp6; goto modified_alpha;
      case ascii_w: key = key_kp7; goto modified_alpha;
      case ascii_x: key = key_kp8; goto modified_alpha;
      case ascii_y: key = key_kp9; goto modified_alpha;
      case ascii_X: key = key_kpeq; goto modified_alpha;

      paste_begin:
        if (params.size() == 1) {
          m_state = decode_paste_default;
          m_paste_data.clear();
          return true;
        }
        break;
      modified_code:
        {
          csi_single_param_t param2, param3;
          params.read_param(param2, 0);
          params.read_param(param3, 0);
          key = param3 & _character_mask;
          if (param2)
            key |= (param2 - 1) << _modifier_shft & _modifier_mask;
          process_key(key);
          return true;
        }
      modified_unicode:
        {
          csi_single_param_t param1;
          params.read_param(param1, 1);
          if (param1 != 1) key = param1 & _character_mask;
          goto modified_key;
        }
      modified_alpha:
        {
          csi_single_param_t param1;
          params.read_param(param1, 1);
          if (param1 == 1) goto modified_key;
          break;
        }
      modified_key:
        {
          csi_single_param_t param2;
          params.read_param(param2, 0);
          if (param2)
            key |= (param2 - 1) << _modifier_shft & _modifier_mask;
          process_key(key);
          return true;
        }
      }
      return false;
    }

  };

}
#endif
