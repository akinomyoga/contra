#include "term_target.h"

class NcursesOutput:public ITarget{
public:
  virtual void flush()=0;
};

