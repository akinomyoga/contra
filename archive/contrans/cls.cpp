#include <windows.h>
#include <stdio.h>
#include <math.h>

#include <hash_map>

#include "cls.h"

#define CALL_STDERR(func,args) do{                       \
	if(!(func args))                                     \
		printf("con: %s:Error %d from %s on line %d\n",  \
			__FILE__, GetLastError(), #func , __LINE__); \
}while(0)                                                /**/

namespace con{

	enum ConsoleColor{
		C_Black		=0,
		C_Red		=FOREGROUND_RED,
		C_Green		=FOREGROUND_GREEN,
		C_Blue		=FOREGROUND_BLUE,
		C_Cyan		=FOREGROUND_GREEN|FOREGROUND_BLUE,
		C_Magenta	=FOREGROUND_RED|FOREGROUND_BLUE,
		C_Yellow	=FOREGROUND_RED|FOREGROUND_GREEN,
		C_White		=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE,
		C_Highlight =FOREGROUND_INTENSITY,
		C_Vertical  =COMMON_LVB_GRID_LVERTICAL|COMMON_LVB_GRID_RVERTICAL,
		C_Underline =COMMON_LVB_UNDERSCORE,
		F_MASK		=0x0F,
		B_MASK		=0xF0,
	};

	struct Csbi{
		HANDLE hConsole;
		CONSOLE_SCREEN_BUFFER_INFO csbi;
	public:
		Csbi(HANDLE hConsole)
			:hConsole(hConsole)
		{
			CALL_STDERR( GetConsoleScreenBufferInfo, (hConsole,&csbi) );
		}
		void ClearRange(const COORD& start,const COORD& end) const{
			DWORD index_e=end.X<0?csbi.dwSize.X*csbi.dwSize.Y: coord2int(end);
			DWORD len=index_e-coord2int(start);
			if(len<=0)return;

			DWORD dummy;
			CALL_STDERR( FillConsoleOutputCharacter, (hConsole,(TCHAR)' ',      len,start,&dummy) );
			CALL_STDERR( FillConsoleOutputAttribute, (hConsole,csbi.wAttributes,len,start,&dummy) );
		}
		void SetCursor(const COORD& coord){
			COORD& c(csbi.dwCursorPosition=coord);
			if(c.X<0)c.X=0;else if(csbi.dwSize.X<=c.X)c.X=csbi.dwSize.X-1;
			if(c.Y<0)c.Y=0;else if(csbi.dwSize.Y<=c.Y)c.Y=csbi.dwSize.Y-1;
			CALL_STDERR( SetConsoleCursorPosition,   (hConsole,c) );
		}
	public:
		const COORD& Cursor() const{
			return this->csbi.dwCursorPosition;
		}
		const COORD& BufSize() const{
			return this->csbi.dwSize;
		}
		WORD GetAttr() const{
			return this->csbi.wAttributes;
		}
		void SetAttr(WORD attr){
			csbi.wAttributes=attr;
			CALL_STDERR( SetConsoleTextAttribute,    (hConsole,csbi.wAttributes) );
		}
		void SetForeground(WORD attr){
			SetAttr(csbi.wAttributes&~F_MASK|attr);
		}
		void SetBackground(WORD attr){
			SetAttr(csbi.wAttributes&~B_MASK|attr<<4);
		}
	private:
		int coord2int(const COORD& c) const{
			return c.X+c.Y*csbi.dwSize.Y;
		}
	};

	//==========================================================================
	// ������
	//==========================================================================
	HANDLE gethandle(HCON hCon=0){
		return hCon?(HANDLE)hCon: GetStdHandle(STD_OUTPUT_HANDLE);
	}

	static WORD default_attr;
	void init(HCON hCon){
		Csbi info(gethandle(hCon));
		default_attr=info.GetAttr();
	}
	void term(HCON hCon){
		Csbi info(gethandle(hCon));
		info.SetAttr(default_attr);
	}

	//==========================================================================
	// ��ʂ̏���
	//==========================================================================
	void cls(HCON hCon){
		Csbi info(gethandle(hCon));
		COORD coordA={0};
		COORD coordZ={-1};
		info.ClearRange(coordA,coordZ);
		info.SetCursor(coordA);
	}
	void cls_b(HCON hCon){
		Csbi info(gethandle(hCon));
		COORD coordA={0};
		info.ClearRange(coordA,info.Cursor());
		// TODO: �J�[�\���̈ʒu�͂ǂ�����̂���ʓI�Ȃ̂�?
	}
	void cls_a(HCON hCon){
		Csbi info(gethandle(hCon));
		COORD coordZ={-1};
		info.ClearRange(info.Cursor(),coordZ);
		// TODO: �J�[�\���̈ʒu�͂ǂ�����̂���ʓI�Ȃ̂�?
	}

	//==========================================================================
	// �s�̏���
	//==========================================================================
	void cls_line(HCON hCon){
		Csbi info(gethandle(hCon));
		COORD coord0={0,                info.Cursor().Y};
		COORD coord1={info.BufSize().X, info.Cursor().Y};
		info.ClearRange(coord0,coord1);
		info.SetCursor(coord0);
	}
	void cls_line_b(HCON hCon){
		Csbi info(gethandle(hCon));
		COORD coord0={0,                info.Cursor().Y};
		info.ClearRange(coord0,info.Cursor());
		// TODO: �J�[�\���̈ʒu�͂ǂ�����̂���ʓI�Ȃ̂�?
	}
	void cls_line_a(HCON hCon){
		Csbi info(gethandle(hCon));
		COORD coord1={info.BufSize().X, info.Cursor().Y};
		info.ClearRange(info.Cursor(),coord1);
		// TODO: �J�[�\���̈ʒu�͂ǂ�����̂���ʓI�Ȃ̂�?
	}

	//==========================================================================
	// �F�̐ݒ�
	//==========================================================================
	void set_color(int argc,int* argv,HCON hCon){
		Csbi info(gethandle(hCon));

		WORD h=0;
		for(int i=0;i<argc;i++){
			int color=argv[i];
			switch(color){
				case 0:info.SetAttr(default_attr);break;
				case 1:h=C_Highlight;continue;
				case 2:{
					WORD a=info.GetAttr();
					info.SetAttr(a|C_Vertical);
					break;
				}
				case 4:{
					WORD a=info.GetAttr();
					info.SetAttr(a|C_Underline);
					break;
				}
				case 7:{
					WORD a=info.GetAttr();
					info.SetAttr((a&B_MASK)>>4|(a&F_MASK)<<4);
					break;
				}
				// �O�i�F
				case 8:
				case 16:info.SetForeground(info.GetAttr()>>4);break; // ��\��
				case 30:info.SetForeground(h|C_Black);break; // ����
				case 17:
				case 31:info.SetForeground(h|C_Red);break; // �Ԏ�
				case 20:
				case 32:info.SetForeground(h|C_Green);break; // �Ύ�
				case 21:
				case 33:info.SetForeground(h|C_Yellow);break; // ����
				case 18:
				case 34:info.SetForeground(h|C_Blue);break; // ��
				case 19:
				case 35:info.SetForeground(h|C_Magenta);break; // ����
				case 22:
				case 36:info.SetForeground(h|C_Cyan);break; // ����
				case 23:
				case 37:info.SetForeground(h|C_White);break; // ����
				// �w�i�F
				case 40:info.SetBackground(h|C_Black);break; // ���n
				case 41:info.SetBackground(h|C_Red);break; // �Ԓn
				case 42:info.SetBackground(h|C_Green);break; // �Βn
				case 43:info.SetBackground(h|C_Yellow);break; // ���n
				case 44:info.SetBackground(h|C_Blue);break; // �n
				case 45:info.SetBackground(h|C_Magenta);break; // ���n
				case 46:info.SetBackground(h|C_Cyan);break; // ���n
				case 47:info.SetBackground(h|C_White);break; // ���n
			}

			h=0;
		}
	}

	//==========================================================================
	// �J�[�\���ړ�
	//==========================================================================
	void set_pos(int x,int y,HCON hCon){
		Csbi info(gethandle(hCon));
		COORD c={x,y};
		info.SetCursor(c);
	}
	void move_u(int d,HCON hCon){
		Csbi info(gethandle(hCon));
		COORD c(info.Cursor());
		c.Y-=d;
		info.SetCursor(c);
	}
	void move_d(int d,HCON hCon){
		Csbi info(gethandle(hCon));
		COORD c(info.Cursor());
		c.Y+=d;
		info.SetCursor(c);
	}
	void move_r(int d,HCON hCon){
		Csbi info(gethandle(hCon));
		COORD c(info.Cursor());
		c.X+=d;
		info.SetCursor(c);
	}
	void move_l(int d,HCON hCon){
		Csbi info(gethandle(hCon));
		COORD c(info.Cursor());
		c.X-=d;
		info.SetCursor(c);
	}

	struct conspos_t{
		COORD curs;
		DWORD attr;
	};
	typedef stdext::hash_map<int,conspos_t> posstore_t;
	posstore_t posstore;

	void store_pos(int id,HCON hCon){
		Csbi info(gethandle(hCon));
		conspos_t pos={info.Cursor(),info.GetAttr()};
		posstore[id]=pos;
	}
	void load_pos(int id,HCON hCon){
		posstore_t::iterator i=posstore.find(id);
		if(i==posstore.end())return;

		Csbi info(gethandle(hCon));
		info.SetCursor(i->second.curs);
		info.SetAttr(i->second.attr);
	}

	//==========================================================================
	// ���t pause/play/piano
	//==========================================================================
	inline double herz(int note){
		return 440*pow(2.0,double(note-69)/12);
	}
	void beep(int argc,int* argv){
		if(argc==1){
			Sleep(argv[0]);
		}else if(argc==2){
			int time=argv[0];
			int hz=int(0.5+herz(argv[1]));
			Beep(hz,time);
		}else if(argc>2){
			const int SLICE=5;

			int time=argv[0];

			// ���g���\
			int n=argc-1;
			int* hz=(int*)calloc(n,sizeof(int));
			for(int i=0;i<n;i++)hz[i]=int(0.5+herz(argv[i+1]));

			int l=0;
			while(time>SLICE){
				Beep(hz[l],SLICE);
				l=(l+1)%n;
				time-=SLICE;
			}
			Beep(hz[l],time);
		}
	}
}