#pragma once
namespace con{
  typedef void* HCON;
  void init(HCON hCon=0);
  void term(HCON hCon=0);

  void cls(HCON hCon=0);
  void cls_a(HCON hCon=0);
  void cls_b(HCON hCon=0);
  void cls_line(HCON hCon=0);
  void cls_line_a(HCON hCon=0);
  void cls_line_b(HCON hCon=0);
  void set_color(int argc,int* argv,HCON hCon=0);
  void beep(int argc,int* argv);
  void set_pos(int x,int y,HCON hCon=0);
  void move_u(int d,HCON hCon=0);
  void move_d(int d,HCON hCon=0);
  void move_r(int d,HCON hCon=0);
  void move_l(int d,HCON hCon=0);

  void store_pos(int id,HCON hCon=0);
  void load_pos(int id,HCON hCon=0);
}
