// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_CONTRADEF_H
#define CONTRA_CONTRADEF_H
#include <cstdint>
namespace contra {
  typedef std::uint8_t byte;

  template<typename T, std::size_t N>
  constexpr std::size_t size(T const (&)[N]) {return N;}

}
#endif
