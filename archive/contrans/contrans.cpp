
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "cls.h"
#include "escseq.h"

using namespace mwg::console;

//==============================================================================
//		Arguments
//==============================================================================
static bool is_esc1ext(byte c){
  return c=='`'||c=='\33';
}

void print_help(){
  Writer w(stdout);
  w.is_esc1=&is_esc1ext;

  FILE* f=fopen("contrans.man","rb");
  foreach_char(f,w);
  fclose(f);
}

class Arguments{
public:
  bool extesc; // ` も ESC として扱うか
public:
  Arguments(){
    extesc=false;
  }
  bool read(int argc,char** argv){
    for(int i=1;i<argc;i++){
      if(argv[i][0]=='-'||argv[i][0]=='/'){
        // option
        char* a=&argv[i][1];
        if(enc_stricmp(a,"?")==0||enc_stricmp(a,"help")==0){
          print_help();
          return false;
        }else if(enc_stricmp(a,"e`")==0){
          extesc=true;
        }else{
          // TODO:
        }
      }else{
        // TODO:
      }
    }
    return true;
  }
} args;

//==============================================================================
//		Main
//==============================================================================
void work2(FILE* ostr,FILE* istr){
  Writer w(ostr);
  if(args.extesc)
    w.is_esc1=&is_esc1ext;

  foreach_char(istr,w);
}

int main(int argc,char** argv){
  setlocale(LC_ALL,"");
  con::init();
  init_handlers();

  bool r=args.read(argc,argv);
  if(r)work2(stdout,stdin);

  con::term();
  return 0;
}
