// -*- mode:C++ -*-
#pragma once
#ifndef CONTRA_TERM_TARGET_H
#define CONTRA_TERM_TARGET_H
#include <vector>
#include <string>
#include <mwg/defs.h>

class InputDecoder;

struct SequenceArgument{
  int type;
  int mode;
  int arg;
  std::vector<int> args;
  std::string text;
public:
  SequenceArgument(){
    this->clear();
  }

private:
  friend class InputDecoder;
  bool process_meta_characters_flag;

  void clear();
  /// \returns whether the sequence is terminated or not.
  bool csi_push(mwg::byte c);
  /// \returns whether the sequence is terminated or not.
  bool apc_push(mwg::byte c);
  /// \returns whether the sequence is terminated or not.
  bool osc_push(mwg::byte c);
};

enum ControlCodes{
  BEL=0x07,
  ESC=0x1b,

  DCS=0x90,
  SOS=0x98,
  CSI=0x9b,
  ST =0x9c,
  OSC=0x9d,
  PM =0x9e,
  APC=0x9f,

  TCC_NUL=0x100,
  // terminal control functions
  TCC_CUU, // CSI, cursor up
  TCC_CUD, // CSI, cursor down
  TCC_CUF, // CSI, cursor forward
  TCC_CUB, // CSI, cursor backward

  TCC_SGR, // CSI, select graphic rendition

  // CNL,CPL,
  // CHA,CUP,

  // CHT,CBT,

  // ED,DECSED,
  // EL,DECSEL,DECSCA,

  // IL,DL,DCH,SU,SD,
  // ECH,HPA,REP,

  // DA1,DA2,
  // VPA,HVP,
  // TBC,

  // DECCARA,DECRARA,
  // SM,RM,DECSET,DECRST,DECSETRestore,DECSETSave,
  // MC,DECMC,

  // DSR,DECDSR,
  
  // DECSTR,DECSCL,

  // DECSTBM,

  // DECCRA,
  // ANSI_SYS_SaveCursor,
  // DECSLPP,
  // DECEFR,DECELR,DECSLE,
  
  // DECREQTPARM,
  // DECSACE,
  // DECFRA,
  // DECERA,DECSERA,
  
};

class ITarget{
public:
  virtual bool process_function(ControlCodes func,SequenceArgument const& arg)=0;
  virtual void putc(mwg::byte data)=0;
  virtual void flush()=0;
};

#endif
