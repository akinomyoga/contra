#ifndef ESCSEQ_H
#define ESCSEQ_H

#include <stdio.h>
#include <stdlib.h>

#include <mbstring.h>
#include <mbctype.h>

#include <vector>
#include <string>

namespace mwg{
namespace console{
  typedef unsigned char byte;

  template<typename F>
  void foreach_char(FILE* istr,const F& f){
    class E{
      const F& f;
      FILE* istr;
      byte *const buff;
      byte *const bufM;
      byte* p;
    private:
      void clear(){
        p=buff;
      }
      bool next(){
        int c=fgetc(istr);
        if(c==EOF)return false;
        *p++=c;
        return true;
      }
    public:
      E(FILE* istr,const F& f)
        :f(f),istr(istr),
        buff(new byte[MB_CUR_MAX]),
        bufM(buff+MB_CUR_MAX)
      {
        while(true){
          clear();

          if(!next())break;

          // 単一文字
          if(!isleadbyte(buff[0])){
            f(buff,p-buff);
            continue;
          }

          // マルチバイト文字
          byte* mbnext;
          do{
            if(!next())break;
            mbnext=_mbsinc(buff);
          }while(p<mbnext);

          f(buff,p-buff);
        }
      }
      ~E(){
        delete[] buff;
      }
    } e(istr,f);
  }

  template<typename F>
  void foreach_char(const char* str,const F& f){
    byte* p =reinterpret_cast<byte*>(const_cast<char*>(str));
    byte* pM=p+strlen(str);
    while(p<pM){
      byte* np=_mbsinc(p);
      f(p,np-p);
      p=np;
    }
  }

  void init_handlers();

  //==============================================================================
  //		Writer
  //==============================================================================
  class Writer{
    FILE* ostr;

    std::string sequence;  // 現在のエスケープシーケンス
    std::string argbuff;   // 引数バッファ
    std::vector<int> args; // 引数

    int mode;

  // 初期化
  public:
    bool (*is_esc1)(byte);
    Writer(FILE* ostr);
  private:
    static bool def_is_esc1(byte c);

  public:
    void operator()(byte* s,int n) const;
  private:
    void write(byte* s,int n);
    void print(byte* s,int n) const;
  private:
    bool add_arg(byte* s,int n);
    void trans_arg();
  private:
    bool exec_seq(byte* s,int n);
    void clear_seq(bool successed=true);
  };

  void fprint(FILE* ostr,FILE* istr);
  void fprint(FILE* ostr,const char* str);

  //==============================================================================
  //		Initializer
  //==============================================================================
  class Init{
  public:
    Init();
    ~Init();
  };
}
}


#endif
