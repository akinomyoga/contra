#include <stdio.h>
#include <io.h>
#include "escseq.h"

//------------------------------------------------------------------------------

enum Note{
	C =0x0010,
	Cs=0x0110,
	D =0x0210,
	Ds=0x0310,
	E =0x0410,
	F =0x0510,
	Fs=0x0610,
	G =0x0710,
	Gs=0x0810,
	A =0x0910,
	As=0x0A10,
	B =0x0B10,
};

int note_code(int note){
	int iO=int(note&0xFF)-0x10;
	int iN=note>>8;
	return 60+iN+iO*12;
}

void play(double len){
	using namespace mwg::console;

	char buff[32];
	sprintf(buff,"\33[%dp",int(240*len));
	fprint(stdout,buff);
}

void play(double len,int n1){
	using namespace mwg::console;
	int nc1=note_code(n1);
	for(int i=0;i<60;i++)
		printf("%c",nc1-24==i?'*':' ');

	char buff[32];
	sprintf(buff,"\33[60D\33[%d;%dp",int(240*len),nc1);
	fprint(stdout,buff);
}

void play(double len,int n1,int n2){
	using namespace mwg::console;
	int nc1=note_code(n1);
	int nc2=note_code(n2);
	for(int i=24;i<60+24;i++)
		printf("%c",nc1==i||nc2==i?'*':' ');

	char buff[32];
	sprintf(buff,"\33[60D\33[%d;%d;%dp",int(240*len),nc1,nc2);
	fprint(stdout,buff);
}

void play(double len,int n1,int n2,int n3){
	using namespace mwg::console;
	int nc1=note_code(n1);
	int nc2=note_code(n2);
	int nc3=note_code(n3);
	for(int i=24;i<60+24;i++)
		printf("%c",nc1==i||nc2==i||nc3==i?'*':' ');

	char buff[32];
	sprintf(buff,"\33[60D\33[%d;%d;%d;%dp",int(240*len),nc1,nc2,nc3);
	fprint(stdout,buff);
}

//------------------------------------------------------------------------------

void play_music(){
	const Note C=Note(::C+0x100);
	const Note D=Note(::D+0x100);
	const Note F=Note(::F+0x100);
	const Note G=Note(::G+0x100);

	play(1.50,C  ,E  ,G  );
	play(0.50,C  ,E  ,G  );
	play(2.50,B-1,D  ,F  );
	play(0.50);
	play(0.50,C  ,E  ,A-1);
	play(0.50,C  ,E  ,E-1);
	play(0.75,::C,D-1);
	play(1.25,G  ,D-1);

	play(0.50,G-2,C-1,C  );
	play(0.50,G-2,C-1,F  );
	play(0.50,G-2,C-1,G  );
	play(0.50,G-2,C-1,B  );
	play(0.50,G-2,C-1,G  );
	play(0.50,G-2,C-1,F  );
	play(0.50,G-2,C-1,C  );
	play(0.50,G-2,C-1,F  );
	play(0.50,A-2,C-1,G  );
	play(0.50,A-2,C-1,B  );
	play(0.50,A-2,C-1,G  );
	play(0.50,A-2,C-1,F  );
	play(0.50,A-2,C-1,C  );
	play(0.50,A-2,C-1,F  );
	play(0.50,A-2,C-1,G  );
	play(0.50,A-2,C-1,B  );

	play(0.50,G-2,B-2,G  );
	play(0.50,G-2,B-2,F  );
	play(0.50,G-2,B-2,D  );
	play(0.50,G-2,B-2,E  );
	play(0.50,G-2,B-2,B-1);
	play(0.50,G-2,B-2,C  );
	play(0.50,G-2,B-2,G-1);
	play(0.50,G-2,B-2,B-1);
	play(0.75,A-2,C-1,C  );
	play(0.25,A-2,C-1,F  );
	play(0.50,A-2,C-1,G  );
	play(0.50,A-2,C-1,B  );
	play(0.50,A-2,C-1,G  );
	play(0.50,A-2,C-1,F  );
	play(0.50,A-2,C-1,C  );
	play(0.25,A-2,C-1,F  );
	play(0.25,A-2,C-1,G  );

	play(0.50,B-2,D-1,G  );
	play(0.50,B-2,D-1,B  );
	play(0.50,B-2,D-1,G  );
	play(0.50,B-2,D-1,F  );
	play(0.50,B-2,D-1,C  );
	play(0.50,B-2,D-1,F  );
	play(0.50,B-2,D-1,G  );
	play(0.50,B-2,D-1,B  );

	// ::C 60 ;        ;       
	// C   61 ; C-1 49 ; C-2 37
	// D   63 ; D-1 51 ; D-2 39
	// E   64 ; E-1 52 ; E-2 40
	// F   66 ; F-1 54 ; F-2 42
	// G   68 ; G-1 56 ; G-2 44
	// A   69 ; A-1 57 ; A-2 45
	// B   71 ; B-1 59 ; B-2 47
}

//------------------------------------------------------------------------------

int main(){
	using namespace mwg::console;
	mwg::console::Init init;

	fprint(stdout,"\n\n\n\n\n\33[5A");
#define BK "\33[2;0;40m "
#define WK "\33[2;1;47m "
	for(int i=0;i<5;i++){
		fprint(stdout,WK BK WK BK WK WK BK WK BK WK BK WK "\33[m");
	}
	printf("\n");
#undef BK
	
	play_music();

	return 0;
}