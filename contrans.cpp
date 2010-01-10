//#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include <vector>

#include "cls.h"

typedef unsigned char byte;

#if 0
	template<typename F>
	void foreach_char2(FILE* istr,const F& f){
		const int BUFSZ=0x10;
		struct u{
			static size_t readfile(byte *const buff,int sz,FILE* istr){
				byte* p=buff;
				byte* pM=p+sz;
				while(p<pM){
					int c=fgetc(istr);
					if(c==EOF)break;
					*p++=(byte)c;
				}

				return p-buff;
			}
			static byte* readbuff(byte* p,byte* pM,const F& f){
				while(p<pM){
					byte* np=_mbsinc(p);
					f(p,np-p); // 処理
					p=np;
				}
				return p;
			}
		};

		byte buff[BUFSZ];
		size_t sz=0;
		size_t n;
		while(n=u::readfile(buff+sz,BUFSZ-sz,istr)){
			sz+=n;
			byte* p=buff;
			byte* pM=buff+sz;
			p=u::readbuff(p,pM-MB_CUR_MAX+1,f);
			sz=pM-p;
			if(sz)memmove(buff,p,sz);
		}

		memset(buff+sz,0,MB_CUR_MAX);
		u::readbuff(buff,buff+sz,f);
	}
#endif

const int BUFSZ=0x10;//0x400;
template<typename F>
void foreach_char(FILE* istr,const F& f){
	struct u{
		const F& f;
		FILE* istr;
		bool eof;

		byte buff[BUFSZ];
		byte* pM;
		byte* pI;
	public:
		u(const F& f,FILE* istr)
			:f(f),istr(istr),eof(false)
		{
			pM=buff;
			pI=buff;
		}
	public:
		void fill(){
			while(pM<buff+BUFSZ){
				int c=fgetc(istr);
				if(c==EOF){
					eof=true;
					return;
				}

				*pM++=c;
			}
		}
		/// <param name="b">
		/// 一文字を構成する byte 数の最大を指定します。
		/// 残り b byte 以上ある場合に読み取りを実行します。
		/// </param>
		void handle(int b){
			while(pI+b<=pM){
				byte* p=_mbsinc(pI);
				f(pI,p-pI); // 処理
				pI=p;
			}

			// 未読み取り部を先頭に移動
			int rest=pM-pI;
			if(rest)memmove(buff,pI,rest);
			pI=buff;
			pM=buff+rest;
		}
	public:
		void exec(){
			while(!eof){
				fill();
				handle(MB_CUR_MAX);
			}

			memset(pM,0,MB_CUR_MAX);
			handle(1);
		}
	} u1(f,istr);
	u1.exec();
}

int main(){
	setlocale(LC_ALL,"");

	class Args{
		FILE* ostr;
		std::vector<int> data;
		std::string str;
		std::string buff;
	public:
		Args(FILE* ostr):ostr(ostr){}
		void clear(){
			data.clear();
			str.clear();
			buff.clear();
		}
	public:
		bool add(byte* s,int n){
			if(n!=1)return false;

			byte c=s[0];
			if('0'<=c&&c<='9'||c==';'){
				str+=c;
				if(c==';')trans_arg();else buff+=c;
				return true;
			}else{
				return false;
			}
		}
	private:
		void trans_arg(){
			int i=atoi(buff.c_str());
			data.push_back(i);
			buff.clear();
		}
	public:
		bool exec(byte* s,int n){
			if(n!=1){
				fprintf(ostr,"\x1b[%s",str.c_str());
				return false;
			}

			switch(s[0]){
				case 'm':
					trans_arg();
					
					return true;
				default:
					return false;
			}
		}
	private:
		bool exec_J(int argc,int* argv) const{
			if(argc!=1||argv[0]!='J')return false;

			gethandle("handle");
			return true;
		}
	} args(stdout);

	int ic;
	int mode=0;
	foreach_char(stdin,[&](byte* s,int n){
		switch(mode){
			// ESC Sequence の開始
			case 0:
				if(n==1&&s[0]=='\x1b'){
					mode=1;
					break;
				}else goto print;
			case 1:
				if(n==1&&s[0]=='['){
					mode=2;
					args.clear();
					break;
				}else{
					mode=0;
					fputc('\x1b',stdout);
					goto print;
				}
			// 引数読み取り・実行
			case 2:{
				if(args.add(s,n))return;
				if(args.exec(s,n))return;
				goto print;
			}
			// 通常文字の出力
			print:
				while(n--)fputc(*s++,stdout);
				break;
		}
	});
	return 0;
}
