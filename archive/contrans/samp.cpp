#include <cstdlib>
#include <cstdio>
#include <windows.h>

void cls(){
  printf("\33[2JHello! World!\n");
  for(int i=0;i<10;i++){
    printf("\33[2KProgressing ... %d",i);
    fflush(stdout);
    Sleep(100);
  }
}

void beeps(){
  Beep(262,250);
  Beep(294,250);
  Beep(330,250);
  Sleep(250);
}

void color(const char* letter){
  // 彩度 A
  printf("\33[1;41;1;31m%s\33[m",letter);
  printf("\33[1;43;1;31m%s\33[m",letter);
  printf("\33[1;43;1;33m%s\33[m",letter);
  printf("\33[1;43;1;32m%s\33[m",letter);
  printf("\33[1;42;1;32m%s\33[m",letter);
  printf("\33[1;42;1;36m%s\33[m",letter);
  printf("\33[1;46;1;36m%s\33[m",letter);
  printf("\33[1;46;1;34m%s\33[m",letter);
  printf("\33[1;44;1;34m%s\33[m",letter);
  printf("\33[1;45;1;34m%s\33[m",letter);
  printf("\33[1;45;1;35m%s\33[m",letter);
  printf("\33[1;45;1;31m%s\33[m",letter);
  printf("\n");

  printf("\33[1;41;3;31m%s\33[m",letter);
  printf("\33[1;43;3;31m%s\33[m",letter);
  printf("\33[1;43;3;33m%s\33[m",letter);
  printf("\33[1;43;3;32m%s\33[m",letter);
  printf("\33[1;42;3;32m%s\33[m",letter);
  printf("\33[1;42;3;36m%s\33[m",letter);
  printf("\33[1;46;3;36m%s\33[m",letter);
  printf("\33[1;46;3;34m%s\33[m",letter);
  printf("\33[1;44;3;34m%s\33[m",letter);
  printf("\33[1;45;3;34m%s\33[m",letter);
  printf("\33[1;45;3;35m%s\33[m",letter);
  printf("\33[1;45;3;31m%s\33[m",letter);
  printf("\n");

  printf("\33[3;41;1;31m%s\33[m",letter);
  printf("\33[3;43;1;31m%s\33[m",letter);
  printf("\33[3;43;1;33m%s\33[m",letter);
  printf("\33[3;43;1;32m%s\33[m",letter);
  printf("\33[3;42;1;32m%s\33[m",letter);
  printf("\33[3;42;1;36m%s\33[m",letter);
  printf("\33[3;46;1;36m%s\33[m",letter);
  printf("\33[3;46;1;34m%s\33[m",letter);
  printf("\33[3;44;1;34m%s\33[m",letter);
  printf("\33[3;45;1;34m%s\33[m",letter);
  printf("\33[3;45;1;35m%s\33[m",letter);
  printf("\33[3;45;1;31m%s\33[m",letter);
  printf("\n");

  printf("\33[3;41;3;31m%s\33[m",letter);
  printf("\33[3;43;3;31m%s\33[m",letter);
  printf("\33[3;43;3;33m%s\33[m",letter);
  printf("\33[3;43;3;32m%s\33[m",letter);
  printf("\33[3;42;3;32m%s\33[m",letter);
  printf("\33[3;42;3;36m%s\33[m",letter);
  printf("\33[3;46;3;36m%s\33[m",letter);
  printf("\33[3;46;3;34m%s\33[m",letter);
  printf("\33[3;44;3;34m%s\33[m",letter);
  printf("\33[3;45;3;34m%s\33[m",letter);
  printf("\33[3;45;3;35m%s\33[m",letter);
  printf("\33[3;45;3;31m%s\33[m",letter);
  printf("\n");

  // 彩度 B
  char w='7';
  char b='0';
  for(int i=0;i<6;i++){
    char c="132654"[i];
    printf("\33[1;4%c;1;3%cm%s\33[m",w,c,letter);
    printf("\33[3;4%c;1;3%cm%s\33[m",w,c,letter);
    printf("\33[1;4%c;1;3%cm%s\33[m",c,b,letter);
    printf("\33[3;4%c;3;3%cm%s\33[m",w,c,letter);
    printf("\33[1;4%c;3;3%cm%s\33[m",b,c,letter);
    printf("\33[3;4%c;3;3%cm%s\33[m",c,b,letter);
    printf("\n");
  }

  // 彩度 C
  printf("\33[1;47;1;37m%s\33[m",letter);
  printf("\33[1;47;3;37m%s\33[m",letter);
  printf("\33[3;47;3;37m%s\33[m",letter);
  printf("\33[3;47;1;30m%s\33[m",letter);
  printf("\33[1;40;1;30m%s\33[m",letter);
  printf("\33[1;40;3;30m%s\33[m",letter);
  printf("\33[3;40;3;30m%s\33[m",letter);
  printf("\n");

}
int main(){
  printf("\33[1;17;4mHello World!\33[m\n");
  printf("\33[40;1;37;4mHello World!\33[m\n");

  color("疇");
  //color("盥");
  //color("瞹");
  //color("礫");
  /*
  printf(
    "\33[240;60p"
    "\33[240;62p"
    "\33[240;64p"
    "\33[240;65p"
    "\33[240;64p"
    "\33[240;62p"
    "\33[240;60p"
    "\33[240p"
    "\33[240;64;60p"
    "\33[240;65;62p"
    "\33[240;67;64p"
    "\33[240;69;65p"
    "\33[240;67;64p"
    "\33[240;65;62p"
    "\33[240;64;60p"
    "\33[240p"
    );
  //*/
  return 0;
}
