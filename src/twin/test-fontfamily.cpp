#include <fstream>
#include <string>
#include <windows.h>
#include "../enc.utf8.hpp"

template<typename XCH>
int xcscpy_s(XCH* dst, std::size_t sz, const XCH* src) {
  if (!sz--) return 1;
  char c;
  while (sz-- && (c = *src++)) *dst++ = c;
  *dst = '\0';
  return 0;
}

struct font_family_proc {
  std::ofstream file;

  font_family_proc(const char* filename): file(filename) {}

  void process(LOGFONT const* lplf, TEXTMETRIC const* lptm, DWORD nFontType) {
    auto const lpelf = reinterpret_cast<ENUMLOGFONTEX const*>(lplf);
    file << "Name: ";
    std::wstring fontname = lpelf->elfLogFont.lfFaceName;
    for (wchar_t w : fontname) contra::encoding::put_u8(w, file);
    file << "\n";
    file << "  FullName: ";
    fontname = lpelf->elfFullName;
    for (wchar_t w : fontname) contra::encoding::put_u8(w, file);
    file << "\n";

    if (nFontType == TRUETYPE_FONTTYPE) {
      auto const lpntm = reinterpret_cast<NEWTEXTMETRICEX const*>(lptm);
      file << "  ntmSizeEM " << lpntm->ntmTm.ntmSizeEM << "\n";
      file << "  ntmCellHeight " << lpntm->ntmTm.ntmCellHeight << "\n";
      file << "  ntmAvgWidth " << lpntm->ntmTm.ntmAvgWidth << "\n";
    }
  }
};

static int CALLBACK EnumFontFamExProc(LOGFONT const* lplf, TEXTMETRIC const* lptm, DWORD nFontType, LPARAM lParam) {
  reinterpret_cast<font_family_proc*>(lParam)->process(lplf, lptm, nFontType);
  return 1;
}

void dump_font_family(const char* filename) {
  HDC hdc = ::GetDC(0);
  font_family_proc proc(filename);
  LOGFONT logfont = {};
  logfont.lfCharSet = DEFAULT_CHARSET;
  //xcscpy_s(logfont.lfFaceName, LF_FACESIZE, TEXT("UD Digi Kyokasho N-R"));
  //xcscpy_s(logfont.lfFaceName, LF_FACESIZE, TEXT("UD デジタル 教科書体 N-R"));
  xcscpy_s(logfont.lfFaceName, LF_FACESIZE, TEXT(""));
  logfont.lfPitchAndFamily = 0;
  ::EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC) &EnumFontFamExProc, reinterpret_cast<LPARAM>(&proc), 0);
  ::ReleaseDC(0, hdc);
}

int main() {
  dump_font_family("test-fontfamily.txt");
  return 0;
}
