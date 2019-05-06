#include <cstdlib>
#include <cassert>
#include <unistd.h>
//#include <mwg/except.h>
#include "term_target.h"
#include "defs.hpp"

ITarget* CreateNcursesInstance(int fd,const char* TERM=nullptr);

class NcursesOutput:public ITarget{
  int fd;
public:
  NcursesOutput(int fd,const char* TERM){
    this->fd=fd;
    assert(TERM!=nullptr&&::isatty(fd));
  }
private:
  bool process_SGR(SequenceArgument const& arg){
    if(arg.size()==0){
      this->putc('\33');
      this->putc('[');
      this->putc('m');
    }
    return true;
  }
public:
  virtual bool process_function(ControlCodes func,SequenceArgument const& arg){
    switch(func){
    TTC_SGR:
      return process_SGR(arg);
    }
    return false;
  }
  virtual void putc(contra1::byte data){
    //■TODO キャッシュ
    ::write(fd,&data,1);
  }
  virtual void flush(){}
};

ITarget* CreateNcursesInstance(int fd,const char* TERM){
  if(TERM==nullptr)
    TERM=std::getenv("TERM");

  if(TERM!=nullptr&&::isatty(fd))
    return new NcursesOutput(fd,TERM);
  else
    return nullptr;
}
