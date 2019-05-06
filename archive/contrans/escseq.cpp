#include <hash_map>

#include "cls.h"
#include "escseq.h"


namespace mwg{
namespace console{
//******************************************************************************

//==============================================================================
//		Handlers
//==============================================================================
typedef bool (*handler_t)(int argc,int* argv);
stdext::hash_map<byte,handler_t> handlers;
typedef stdext::hash_map<byte,handler_t> handlers_t;
void init_handlers(){
	struct h{
		static bool exec_m(int argc,int* argv){
			con::set_color(argc,argv);
			return true;
		}
		static bool exec_p(int argc,int* argv){
			con::beep(argc,argv);
			return true;
		}
		static bool exec_J(int argc,int* argv){
			if(argc!=1)return false;
			switch(argv[0]){
				case 0:
					con::cls_a();
					return true;
				case 1:
					con::cls_b();
					return true;
				case 2:
					con::cls();
					return true;
				default:
					return false;
			}
		}
		static bool exec_K(int argc,int* argv){
			if(argc!=1)return false;
			switch(argv[0]){
				case 0:
					con::cls_line_a();
					return true;
				case 1:
					con::cls_line_b();
					return true;
				case 2:
					con::cls_line();
					return true;
				default:
					return false;
			}
		}
		static bool exec_H(int argc,int* argv){
			if(argc!=2)return false;
			con::set_pos(argv[0],argv[1]);
			return true;
		}
		static bool exec_A(int argc,int* argv){
			if(argc!=1)return false;
			con::move_u(argv[0]);
			return true;
		}
		static bool exec_B(int argc,int* argv){
			if(argc!=1)return false;
			con::move_d(argv[0]);
			return true;
		}
		static bool exec_C(int argc,int* argv){
			if(argc!=1)return false;
			con::move_r(argv[0]);
			return true;
		}
		static bool exec_D(int argc,int* argv){
			if(argc!=1)return false;
			con::move_l(argv[0]);
			return true;
		}
		static bool exec_s(int argc,int* argv){
			if(argc!=1)return false;
			con::store_pos(argv[0]);
			return true;
		}
		static bool exec_u(int argc,int* argv){
			if(argc!=1)return false;
			con::load_pos(argv[0]);
			return true;
		}
	};

	// クリア
	handlers['J']=&h::exec_J;
	handlers['K']=&h::exec_K;
	handlers['k']=&h::exec_K;

	// カーソル移動
	handlers['A']=&h::exec_A;
	handlers['B']=&h::exec_B;
	handlers['C']=&h::exec_C;
	handlers['D']=&h::exec_D;
	handlers['H']=&h::exec_H;
	handlers['f']=&h::exec_H;
	handlers['s']=&h::exec_s;
	handlers['u']=&h::exec_u;

	// 他
	handlers['m']=&h::exec_m;
	handlers['p']=&h::exec_p;
}
//==============================================================================
//		Writer
//==============================================================================
Writer::Writer(FILE* ostr):ostr(ostr){
	mode=0;
	is_esc1=&Writer::def_is_esc1;
}
bool Writer::def_is_esc1(byte c){
	return c=='\x1b';
}
//------------------------------------------------------------------------------
// メインループ
inline void Writer::write(byte* s,int n){
	switch(mode){
		// ESC Sequence の開始
		case 0:
			if(n==1&&is_esc1(s[0])){
				mode=1;
				sequence+=s[0];
			}else{
				print(s,n);
			}
			break;
		case 1:
			if(n==1&&s[0]=='['){
				mode=2;
				sequence+=s[0];
			}else{
				mode=0;
				clear_seq(false);
				print(s,n);
			}
			break;
		// 引数読み取り・実行
		case 2:{
			if(add_arg(s,n))break;
			mode=0;
			if(exec_seq(s,n))break;
			print(s,n);
			break;
		}
	}
}
inline void Writer::print(byte* s,int n) const{
	while(n--)fputc(*s++,ostr);
}
// ESC 引数
inline bool Writer::add_arg(byte* s,int n){
	if(n!=1)return false;

	byte c=s[0];
	if('0'<=c&&c<='9'||c==';'){
		if(c==';')trans_arg();else argbuff+=c;
		sequence+=c;
		return true;
	}else{
		return false;
	}
}
inline void Writer::trans_arg(){
	int i=atoi(argbuff.c_str());
	args.push_back(i);
	argbuff.clear();
}
// ESC SEQ 完了
inline bool Writer::exec_seq(byte* s,int n){
	bool suc=false;
	if(n==1){
		handlers_t::iterator i=handlers.find(s[0]);
		if(i!=handlers.end()){
			trans_arg();
			suc=i->second(args.size(),&args[0]);
		}
	}

	clear_seq(suc);
	return suc;
}
inline void Writer::clear_seq(bool successed){
	if(!successed)fprintf(ostr,"%s",sequence.c_str());
	sequence.clear();
	argbuff.clear();
	args.clear();
}
//------------------------------------------------------------------------------
void Writer::operator()(byte* s,int n) const{
	const_cast<Writer*>(this)->write(s,n);
}

//==============================================================================
void fprint(FILE* ostr,FILE* istr){
	Writer w(ostr);
	foreach_char(istr,w);
}
void fprint(FILE* ostr,const char* str){
	Writer w(ostr);
	foreach_char(str,w);
}

Init::Init(){
	setlocale(LC_ALL,"");
	con::init();
	init_handlers();
}
Init::~Init(){
	con::term();
}

//******************************************************************************
}
}
