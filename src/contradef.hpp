// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_contradef_hpp
#define contra_contradef_hpp
#include <cstddef>
#include <cstdint>
#include <vector>
namespace contra {
  typedef std::uint8_t byte;

  template<typename T>
  struct identity { typedef T type; };

  // // std::size (C++17 <iterator>)
  // template<typename T, std::size_t N>
  // constexpr std::size_t size(T const (&)[N]) {return N;}

  template<typename T>
  constexpr T clamp(T value, typename identity<T>::type min, typename identity<T>::type max) {
    return value < min ? min : value > max ? max : value;
  }

  // discarded expression
#define contra_unused(X) static_cast<void>(X)

  //---------------------------------------------------------------------------
  // Output devices

  struct idevice {
    virtual void dev_write(char const* data, std::size_t size) = 0;
    virtual ~idevice() {}
  };

  class multicast_device: public idevice {
    std::vector<idevice*> m_list;
  public:
    void push(idevice* dev) {this->m_list.push_back(dev);}
    virtual void dev_write(char const* data, std::size_t size) override {
      for (idevice* const dev: m_list)
        dev->dev_write(data, size);
    }
  };

  //---------------------------------------------------------------------------
  // ASCII codes

  enum ascii_codes {

    // control characters

    ascii_nul = 0x00, ascii_dle = 0x10, ascii_pad = 0x80, ascii_dcs  = 0x90,
    ascii_soh = 0x01, ascii_dc1 = 0x11, ascii_hop = 0x81, ascii_pu1  = 0x91,
    ascii_stk = 0x02, ascii_dc2 = 0x12, ascii_bph = 0x82, ascii_pu2  = 0x92,
    ascii_etx = 0x03, ascii_dc3 = 0x13, ascii_nbh = 0x83, ascii_sts  = 0x93,
    ascii_eot = 0x04, ascii_dc4 = 0x14, ascii_ind = 0x84, ascii_cch  = 0x94,
    ascii_enq = 0x05, ascii_nak = 0x15, ascii_nel = 0x85, ascii_mw   = 0x95,
    ascii_ack = 0x06, ascii_syn = 0x16, ascii_ssa = 0x86, ascii_spa  = 0x96,
    ascii_bel = 0x07, ascii_etb = 0x17, ascii_esa = 0x87, ascii_epa  = 0x97,
    ascii_bs  = 0x08, ascii_can = 0x18, ascii_hts = 0x88, ascii_sos  = 0x98,
    ascii_ht  = 0x09, ascii_em  = 0x19, ascii_htj = 0x89, ascii_sgci = 0x99,
    ascii_lf  = 0x0A, ascii_sub = 0x1A, ascii_vts = 0x8A, ascii_sci  = 0x9A,
    ascii_vt  = 0x0B, ascii_esc = 0x1B, ascii_pld = 0x8B, ascii_csi  = 0x9B,
    ascii_ff  = 0x0C, ascii_fs  = 0x1C, ascii_plu = 0x8C, ascii_st   = 0x9C,
    ascii_cr  = 0x0D, ascii_gs  = 0x1D, ascii_ri  = 0x8D, ascii_osc  = 0x9D,
    ascii_so  = 0x0E, ascii_rs  = 0x1E, ascii_ss2 = 0x8E, ascii_pm   = 0x9E,
    ascii_si  = 0x0F, ascii_us  = 0x1F, ascii_ss3 = 0x8F, ascii_apc  = 0x9F,

    ascii_sp   = 0x20,
    ascii_del  = 0x7F,
    ascii_nbsp = 0xA0,

    // alnum

    ascii_0 = 0x30,
    ascii_1 = 0x31,
    ascii_2 = 0x32,
    ascii_3 = 0x33,
    ascii_4 = 0x34,
    ascii_5 = 0x35,
    ascii_6 = 0x36,
    ascii_7 = 0x37,
    ascii_8 = 0x38,
    ascii_9 = 0x39,

    ascii_A = 0x41, ascii_a = 0x61,
    ascii_B = 0x42, ascii_b = 0x62,
    ascii_C = 0x43, ascii_c = 0x63,
    ascii_D = 0x44, ascii_d = 0x64,
    ascii_E = 0x45, ascii_e = 0x65,
    ascii_F = 0x46, ascii_f = 0x66,
    ascii_G = 0x47, ascii_g = 0x67,
    ascii_H = 0x48, ascii_h = 0x68,
    ascii_I = 0x49, ascii_i = 0x69,
    ascii_J = 0x4A, ascii_j = 0x6A,
    ascii_K = 0x4B, ascii_k = 0x6B,
    ascii_L = 0x4C, ascii_l = 0x6C,
    ascii_M = 0x4D, ascii_m = 0x6D,
    ascii_N = 0x4E, ascii_n = 0x6E,
    ascii_O = 0x4F, ascii_o = 0x6F,
    ascii_P = 0x50, ascii_p = 0x70,
    ascii_Q = 0x51, ascii_q = 0x71,
    ascii_R = 0x52, ascii_r = 0x72,
    ascii_S = 0x53, ascii_s = 0x73,
    ascii_T = 0x54, ascii_t = 0x74,
    ascii_U = 0x55, ascii_u = 0x75,
    ascii_V = 0x56, ascii_v = 0x76,
    ascii_W = 0x57, ascii_w = 0x77,
    ascii_X = 0x58, ascii_x = 0x78,
    ascii_Y = 0x59, ascii_y = 0x79,
    ascii_Z = 0x5A, ascii_z = 0x7A,

    // punctuations

    ascii_exclamation   = 0x21,
    ascii_double_quote  = 0x22,
    ascii_number        = 0x23,
    ascii_dollar        = 0x24,
    ascii_percent       = 0x25,
    ascii_ampersand     = 0x26,
    ascii_single_quote  = 0x27,
    ascii_left_paren    = 0x28,
    ascii_right_paren   = 0x29,
    ascii_asterisk      = 0x2A, ascii_colon     = 0x3A,
    ascii_plus          = 0x2B, ascii_semicolon = 0x3B,
    ascii_comma         = 0x2C, ascii_less      = 0x3C,
    ascii_minus         = 0x2D, ascii_equals    = 0x3D,
    ascii_dot           = 0x2E, ascii_greater   = 0x3E,
    ascii_slash         = 0x2F, ascii_question  = 0x3F,

    ascii_at            = 0x40, ascii_back_quote   = 0x60,
    ascii_left_bracket  = 0x5B, ascii_left_brace   = 0x7B,
    ascii_backslash     = 0x5C, ascii_vertical_bar = 0x7C,
    ascii_right_bracket = 0x5D, ascii_right_brace  = 0x7D,
    ascii_circumflex    = 0x5E, ascii_tilde        = 0x7E,
    ascii_underscore    = 0x5F,
  };

  const char* get_ascii_name(char32_t value);

  //---------------------------------------------------------------------------
  // prototype/forward declaration

  namespace ansi {
    struct cell_t;
    class line_t;
    struct board_t;
    class term_t;

    typedef std::int32_t coord_t;
    typedef std::int32_t curpos_t;
  }

  //---------------------------------------------------------------------------
  // Implementation limitations

  namespace limit {
    constexpr ansi::curpos_t minimal_terminal_col = 1;
    constexpr ansi::curpos_t minimal_terminal_row = 1;
    constexpr ansi::curpos_t maximal_terminal_col = 2048;
    constexpr ansi::curpos_t maximal_terminal_row = 2048;
    constexpr ansi::curpos_t minimal_terminal_xpixel = 4; // SGR装飾の類で仮定?
    constexpr ansi::curpos_t minimal_terminal_ypixel = 4; // SGR装飾の類で仮定?
    constexpr ansi::curpos_t maximal_terminal_xpixel = 512;
    constexpr ansi::curpos_t maximal_terminal_ypixel = 512;
    constexpr std::size_t maximal_cells_per_line = maximal_terminal_col * 5;
  }
}
#endif
