
//==============================================================================
//		cls.cpp
//==============================================================================

/* Standard error macro for reporting API errors */ 
#define PERR(bSuccess, api) do{                         \
	if(!(bSuccess))                                     \
		printf("con: %s:Error %d from %s on line %d\n", \
			__FILE__, GetLastError(), api, __LINE__);   \
}while(0)                                               /**/

namespace con{
	//==========================================================================
	// 画面の消去
	//==========================================================================
	void cls(HCON hCon){
		HANDLE hConsole=(HANDLE)hCon;

		COORD coordScreen = { 0, 0 };    /* here's where we'll home the cursor */ 
		BOOL bSuccess;
		DWORD cCharsWritten;
		CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */ 
		DWORD dwConSize;                 /* number of character cells in the current buffer */ 

		/* get the number of character cells in the current buffer */ 
		bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );
		PERR( bSuccess, "GetConsoleScreenBufferInfo" );
		dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

		/* fill the entire screen with blanks */ 
		bSuccess = FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten );
		PERR( bSuccess, "FillConsoleOutputCharacter" );

		/* get the current text attribute */ 
		//bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );
		//PERR( bSuccess, "ConsoleScreenBufferInfo" );

		/* now set the buffer's attributes accordingly */ 
		bSuccess = FillConsoleOutputAttribute(hConsole,csbi.wAttributes,dwConSize, coordScreen, &cCharsWritten );
		PERR( bSuccess, "FillConsoleOutputAttribute" );

		/* put the cursor at (0, 0) */ 
		bSuccess = SetConsoleCursorPosition( hConsole, coordScreen );
		PERR( bSuccess, "SetConsoleCursorPosition" );
		return;
	}

	void cls_b(HCON hCon){
		HANDLE hConsole=(HANDLE)hCon;

		CONSOLE_SCREEN_BUFFER_INFO csbi;
		CALL_STDERR( GetConsoleScreenBufferInfo, (hConsole,&csbi) );
		DWORD len=csbi.dwCursorPosition.Y*csbi.dwSize.Y+csbi.dwCursorPosition.X;

		DWORD dummy;
		COORD coord0={0};
		CALL_STDERR( FillConsoleOutputCharacter, (hConsole,(TCHAR)' ',      len,coord0,&dummy) );
		CALL_STDERR( FillConsoleOutputAttribute, (hConsole,csbi.wAttributes,len,coord0,&dummy) );

		// TODO: カーソルの位置はどうするのが一般的なのか?
	}

	void cls_a(HCON hCon){
		HANDLE hConsole=(HANDLE)hCon;

		CONSOLE_SCREEN_BUFFER_INFO csbi;
		CALL_STDERR( GetConsoleScreenBufferInfo, (hConsole,&csbi) );
		DWORD cursor =csbi.dwCursorPosition.Y*csbi.dwSize.Y+csbi.dwCursorPosition.X;
		DWORD total  =csbi.dwSize.X*csbi.dwSize.Y;

		DWORD dummy;
		CALL_STDERR( FillConsoleOutputCharacter, (hConsole,(TCHAR)' ',      total-cursor,csbi.dwCursorPosition,&dummy) );
		CALL_STDERR( FillConsoleOutputAttribute, (hConsole,csbi.wAttributes,total-cursor,csbi.dwCursorPosition,&dummy) );

		// TODO: カーソルの位置はどうするのが一般的なのか?
	}

	//==========================================================================
	// 行の消去
	//==========================================================================
	void cls_line(HCON hCon){
		HANDLE hConsole=(HANDLE)hCon;

		CONSOLE_SCREEN_BUFFER_INFO csbi;
		CALL_STDERR( GetConsoleScreenBufferInfo, (hConsole,&csbi) );
		DWORD len=csbi.dwSize.Y;

		DWORD dummy;
		COORD coord0={0,csbi.dwCursorPosition.Y};
		CALL_STDERR( FillConsoleOutputCharacter, (hConsole,(TCHAR)' ',      len,coord0,&dummy) );
		CALL_STDERR( FillConsoleOutputAttribute, (hConsole,csbi.wAttributes,len,coord0,&dummy) );

		CALL_STDERR( SetConsoleCursorPosition, (hConsole,coord0) );
	}
	void cls_line_b(HCON hCon){
		HANDLE hConsole=(HANDLE)hCon;

		CONSOLE_SCREEN_BUFFER_INFO csbi;
		CALL_STDERR( GetConsoleScreenBufferInfo, (hConsole,&csbi) );
		DWORD len=csbi.dwCursorPosition.X;

		DWORD dummy;
		COORD coord0={0,csbi.dwCursorPosition.Y};
		CALL_STDERR( FillConsoleOutputCharacter, (hConsole,(TCHAR)' ',      len,coord0,&dummy) );
		CALL_STDERR( FillConsoleOutputAttribute, (hConsole,csbi.wAttributes,len,coord0,&dummy) );

		// TODO: カーソルの位置はどうするのが一般的なのか?
	}
	void cls_line_a(HCON hCon){
		HANDLE hConsole=(HANDLE)hCon;

		CONSOLE_SCREEN_BUFFER_INFO csbi;
		CALL_STDERR( GetConsoleScreenBufferInfo, (hConsole,&csbi) );
		DWORD len=csbi.dwSize.Y-csbi.dwCursorPosition.X;

		DWORD dummy;
		CALL_STDERR( FillConsoleOutputCharacter, (hConsole,(TCHAR)' ',      len,csbi.dwCursorPosition,&dummy) );
		CALL_STDERR( FillConsoleOutputAttribute, (hConsole,csbi.wAttributes,len,csbi.dwCursorPosition,&dummy) );

		// TODO: カーソルの位置はどうするのが一般的なのか?
	}

}

//==============================================================================
//		contrans2.cpp
//==============================================================================
void work(FILE* ostr){
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

			handlers_t::iterator i=handlers.find(s[0]);
			if(i!=handlers.end()){
				trans_arg();
				return i->second(data.size(),&data[0]);
			}else{
				return false;
			}
		}
	} args(ostr);

	int ic;
	int mode=0;
	foreach_char(stdin,[&,ostr](byte* s,int n){
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
					fputc('\x1b',ostr);
					goto print;
				}
			// 引数読み取り・実行
			case 2:{
				if(args.add(s,n))return;
				mode=0;
				if(args.exec(s,n))return;
				goto print;
			}
			// 通常文字の出力
			print:
				while(n--)fputc(*s++,ostr);
				break;
		}
	});
}

