#include "term.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <mwg/except.h>
#include "../sequence.h"

namespace contra {
namespace ansi {
namespace {

  class csi_parameters;

  typedef bool control_function_t(tty_player& play, csi_parameters& params);

  typedef std::uint32_t csi_single_param_t;

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
    csi_parameters(sequence const& seq) {
      extract_parameters(seq.parameter(), seq.parameterSize());
    }

    // csi_parameters(std::initializer_list<csi_single_param_t> args) {
    //   for (csi_single_param_t value: args)
    //     this->push_back({value, false, false});
    // }

    operator bool() const {return this->m_result;}

  public:
    std::size_t size() const {return m_data.size();}
    void push_back(csi_param_type const& value) {m_data.push_back(value);}

  private:
    bool extract_parameters(char32_t const* str, std::size_t len) {
      m_data.clear();
      m_index = 0;
      m_isColonAppeared = false;
      m_result = false;

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
      if (m_index <m_data.size()
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
  };

  // tty_player.cpp に実装がある物:
  // bool do_spd(tty_player& play, csi_parameters& params);
  // bool do_scp(tty_player& play, csi_parameters& params);
  // bool do_simd(tty_player& play, csi_parameters& params);
  bool do_sds(tty_player& play, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;
    play.insert_marker(
      param == 1 ? character_t::marker_sds_l2r :
      param == 2 ? character_t::marker_sds_r2l :
      character_t::marker_sds_end);
    return true;
  }
  bool do_srs(tty_player& play, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;
    play.insert_marker(
      param == 1 ? character_t::marker_srs_beg :
      character_t::marker_srs_end);
    return true;
  }
  // bool do_slh(tty_player& play, csi_parameters& params);
  // bool do_sll(tty_player& play, csi_parameters& params);
  // bool do_sph(tty_player& play, csi_parameters& params);
  // bool do_spl(tty_player& play, csi_parameters& params);
  // bool do_cuu(tty_player& play, csi_parameters& params)
  // bool do_cud(tty_player& play, csi_parameters& params);
  // bool do_cuf(tty_player& play, csi_parameters& params);
  // bool do_cub(tty_player& play, csi_parameters& params);
  // bool do_hpb(tty_player& play, csi_parameters& params);
  // bool do_hpr(tty_player& play, csi_parameters& params);
  // bool do_vpb(tty_player& play, csi_parameters& params);
  // bool do_vpr(tty_player& play, csi_parameters& params);
  // bool do_cnl(tty_player& play, csi_parameters& params);
  // bool do_cpl(tty_player& play, csi_parameters& params);
  // bool do_cup(tty_player& play, csi_parameters& params);
  // bool do_cha(tty_player& play, csi_parameters& params);
  // bool do_hpa(tty_player& play, csi_parameters& params);
  // bool do_vpa(tty_player& play, csi_parameters& params);
  bool do_sgr(tty_player& play, csi_parameters& params) { (void) play; (void) params; return false; }
  // bool do_sco(tty_player& play, csi_parameters& params);
  // bool do_ech(tty_player& play, csi_parameters& params);

  constexpr std::uint32_t compose_bytes(byte major, byte minor) {
    return minor << 8 | major;
  }

  struct control_function_dictionary {
    control_function_t* data1[63];
    std::unordered_map<std::uint16_t, control_function_t*> data2;

    void register_cfunc(control_function_t* fp, byte F) {
      int const index = (int) F - ascii_at;
      mwg_assert(0 <= index && index < (int) size(data1), "final byte out of range");
      mwg_assert(data1[index] == nullptr, "another function is already registered");
      data1[index] = fp;
    }

    void register_cfunc(control_function_t* fp, byte I, byte F) {
      data2[compose_bytes(I, F)] = fp;
    }

    control_function_dictionary() {
      std::fill(std::begin(data1), std::end(data1), nullptr);

      // // sgr
      // register_cfunc(&do_sgr, ascii_m);

      // // cursor movement
      // register_cfunc(&do_cuu, ascii_A);
      // register_cfunc(&do_cud, ascii_B);
      // register_cfunc(&do_cuf, ascii_C);
      // register_cfunc(&do_cub, ascii_D);
      // register_cfunc(&do_hpb, ascii_j);
      // register_cfunc(&do_hpr, ascii_a);
      // register_cfunc(&do_vpb, ascii_k);
      // register_cfunc(&do_vpr, ascii_e);
      // register_cfunc(&do_cnl, ascii_E);
      // register_cfunc(&do_cpl, ascii_F);

      // // cursor position
      // register_cfunc(&do_cha, ascii_G);
      // register_cfunc(&do_cup, ascii_H);
      // register_cfunc(&do_hpa, ascii_back_quote);
      // register_cfunc(&do_vpa, ascii_d);

      // // ECH/DCH/ICH, etc.
      // register_cfunc(&do_ech, ascii_X);

      // // implicit movement
      // register_cfunc(&do_simd, ascii_circumflex);

      // bidi strings
      register_cfunc(&do_sds, ascii_right_bracket);
      register_cfunc(&do_srs, ascii_left_bracket);

      // // presentation/line directions
      // register_cfunc(&do_spd, ascii_sp, ascii_S);
      // register_cfunc(&do_scp, ascii_sp, ascii_k);

      // // line/page limits
      // register_cfunc(&do_slh, ascii_sp, ascii_U);
      // register_cfunc(&do_sll, ascii_sp, ascii_V);
      // register_cfunc(&do_sph, ascii_sp, ascii_i);
      // register_cfunc(&do_spl, ascii_sp, ascii_j);

      // register_cfunc(&do_sco, ascii_sp, ascii_e);
    }

    control_function_t* get(byte F) const {
      int const index = (int) F - ascii_at;
      return 0 <= index && index < (int) size(data1) ? data1[index] : nullptr;
    }

    control_function_t* get(byte I, byte F) const {
      typedef std::unordered_map<std::uint16_t, control_function_t*>::const_iterator const_iterator;
      const_iterator const it = data2.find(compose_bytes(I, F));
      return it != data2.end() ? it->second: nullptr;
    }
  };

  static control_function_dictionary cfunc_dict;

}

  void tty_player::process_control_sequence(sequence const& seq) {
    if (seq.is_private_csi()) {
      print_unrecognized_sequence(seq);
      return;
    }

    csi_parameters params(seq);
    if (!params) {
      print_unrecognized_sequence(seq);
      return;
    }

    bool result = false;
    std::int32_t const intermediateSize = seq.intermediateSize();
    if (intermediateSize == 0) {
      if (seq.final() == ascii_m)
        result = do_sgr(*this, params);
      else if (control_function_t* const f = cfunc_dict.get(seq.final()))
        result = f(*this, params);
    } else if (intermediateSize == 1) {
      mwg_assert(seq.intermediate()[0] <= 0xFF);
      if (control_function_t* const f = cfunc_dict.get((byte) seq.intermediate()[0], seq.final()))
        result = f(*this, params);
    }

    if (!result)
      print_unrecognized_sequence(seq);
  }

}
}
