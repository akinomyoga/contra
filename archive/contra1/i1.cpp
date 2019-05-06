#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include "term_target.h"
#include "defs.hpp"

inline void SequenceArgument::clear(){
  this->type=0;
  this->mode=0;
  this->arg=0;
  args.clear();
}
inline bool SequenceArgument::csi_push(contra1::byte c){
  if(' '<=c&&c<'@'){
    if('0'<=c&&c<='9'){
      this->arg=this->arg*10+(c-'0');
      this->mode=1;
    }else if(c==';'){
      if(this->mode){
        this->args.push_back(this->arg);
        this->arg=0;
        this->mode=0;
      }
    }else{
      this->type=this->type<<8|c;
    }
    return false;
  }else{
    if(this->mode){
      this->args.push_back(this->arg);
      this->arg=0;
      this->mode=0;
    }

    this->type=this->type<<8|c;
    return true;
  }
}
inline bool SequenceArgument::apc_push(contra1::byte c){
  if(this->mode==ESC){
    if(c=='\\')
      return true;

    this->text+=(char)ESC;
    this->mode=0;
  }

  if(c==ST&&process_meta_characters_flag)
    return true;

  if(c==ESC)
    this->mode=ESC;
  else
    this->text+=(char)c;
  return false;
}
inline bool SequenceArgument::osc_push(contra1::byte c){
  if(this->mode==0){
    if('0'<=c&&c<='9'){
      this->type=this->type*10+(c-'0');
      return false;
    }

    this->mode=1;

    if(c==';')
      return false;
  }

  if(this->mode==ESC){
    if(c=='\\')
      return true;

    this->text+=(char)ESC;
    this->mode=1;
  }

  if(c==ST&&process_meta_characters_flag||c==BEL)
    return false;

  if(c==ESC)
    this->mode=ESC;
  else
    this->text+=(char)c;

  return false;
}

class InputDecoder{
  bool process_meta_characters_flag;
public:
  InputDecoder(ITarget* target,bool processMetaCharacters=false)
    :target(target),
     process_meta_characters_flag(processMetaCharacters)
  {
    this->arg.process_meta_characters_flag=processMetaCharacters;
  }
private:
  int mode;
  std::string seq;
  SequenceArgument arg;

  ITarget* target;

  enum Mode{
    Normal,
    Escape,
    CSISequence,
    OSCSequence,
    DCSSequence,
    SOSSequence,
    PMSequence,
    APCSequence,
  };

public:
  void consume(const contra1::byte* begin,const contra1::byte* end){
    int mode=this->mode;

    for(const contra1::byte* p=begin;p<end;p++){
      contra1::byte c=*p;
      switch(mode){
      case Normal:
        if((c&0x7F)>=0x20){
          // most frequent control path
          this->target->putc(c);
          continue;
        }

        if(c==ESC){
          seq+=(char)c;
          mode=Escape;
          continue;
        }

        if(c&0x80&&process_meta_characters_flag){
          switch(c){
          case CSI:
            mode=CSISequence;
            goto start_sequence;
          case OSC:
            mode=OSCSequence;
            goto start_sequence;
          case DCS:
            mode=DCSSequence;
            goto start_sequence;
          case SOS:
            mode=SOSSequence;
            goto start_sequence;
          case PM:
            mode=PMSequence;
            goto start_sequence;
          case APC:
            mode=APCSequence;
            goto start_sequence;
          start_sequence:
            seq+=(char)c;
            continue;
          }
        }

        this->target->putc(c);
        break;
      case Escape:
        seq+=(char)c;
        switch(c){
        case 'P':mode=DCSSequence;break;
        case 'X':mode=SOSSequence;break;
        case '[':mode=CSISequence;break;
        case ']':mode=OSCSequence;break;
        case '^':mode=PMSequence; break;
        case '_':mode=APCSequence;break;
        case '7':
        case '8':
          this->ProcessSingleEscape(c);
          goto end_sequence;
        default:
          this->target->putc(ESC);
          this->target->putc(c);
          goto end_sequence;
        }
        break;
      case CSISequence:
        seq+=(char)c;
        if(this->arg.csi_push(c)){
          this->ProcessCSISequence();
          goto end_sequence;
        }
        break;
      case OSCSequence:
        if(this->arg.osc_push(c)){
          this->ProcessOSCSequence();
          goto end_sequence;
        }
        break;
      case DCSSequence:
        seq+=(char)c;
        if(this->arg.apc_push(c)){
          this->ProcessDCSSequence();
          goto end_sequence;
        }
        break;
      case SOSSequence:
        if(this->arg.apc_push(c)){
          this->ProcessSOSSequence();
          goto end_sequence;
        }
        break;
      case PMSequence:
        seq+=(char)c;
        if(this->arg.apc_push(c)){
          this->ProcessPMSequence();
          goto end_sequence;
        }
        break;
      case APCSequence:
        seq+=(char)c;
        if(this->arg.apc_push(c)){
          this->ProcessAPCSequence();
          goto end_sequence;
        }
        break;
      end_sequence:
        this->seq.clear();
        this->arg.clear();
        mode=Normal;
        break;
      default:
        std::fprintf(stderr,"unexpected state: mode\n");
        std::fflush(stderr);
        std::exit(1);
      }
    }

    this->mode=mode;
  }

private:
  void ProcessSingleEscape(contra1::byte c){
    // this->target->single_escape(c);
  }
  bool ProcessCSISequence(){
    ControlCodes fun=(ControlCodes)0;
    switch(this->arg.type){
    case 'm':fun=TCC_SGR;break;
    // case 'h':fun=SM;break;
    // case 'l':fun=RM;break;
    case 'A':fun=TCC_CUU;break;
    case 'B':fun=TCC_CUD;break;
    case 'C':fun=TCC_CUF;break;
    case 'D':fun=TCC_CUB;break;
    }
    if(fun){
      this->target->process_function(fun,this->arg);
      return true;
    }
    return false;
  }
  void ProcessOSCSequence(){
    this->output_raw_sequence();
  }
  void ProcessDCSSequence(){
    this->output_raw_sequence();
  }
  void ProcessSOSSequence(){
    this->output_raw_sequence();
  }
  void ProcessPMSequence(){
    this->output_raw_sequence();
  }
  void ProcessAPCSequence(){
    this->output_raw_sequence();
  }
  void output_raw_sequence(){
    for(std::size_t i=0,iN=this->seq.length();i<iN;i++)
      this->target->putc(i);
  }
};


class DebugTarget:public ITarget{
public:
  virtual bool process_function(ControlCodes func,SequenceArgument const& arg){
    std::printf("[escseq: %d]",(int)func);
  }
  virtual void putc(contra1::byte data){
    std::putchar((char)data);
  }
  virtual void flush(){
    std::fflush(stdout);
  }
};

int main(){
  DebugTarget t;
  InputDecoder reader(&t);
  const contra1::byte* text=(const contra1::byte*)"\33[1;31mhello world\33[m\n";
  reader.consume(text,text+std::strlen((const char*)text));
  return 0;
}
