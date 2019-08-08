#include <cstdio>
#include "enc.utf8.hpp"

char32_t const iso_ir_146_table[96] =
  {
   0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
   0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
   0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
   0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
   0x0416, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
   0x0425, 0x0418, 0x0408, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
   0x041F, 0x0409, 0x0420, 0x0421, 0x0422, 0x0423, 0x0412, 0x040A,
   0x040F, 0x0405, 0x0417, 0x0428, 0x0402, 0x040B, 0x0427, 0x005F,
   0x0436, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
   0x0445, 0x0438, 0x0458, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
   0x043F, 0x0459, 0x0440, 0x0441, 0x0442, 0x0443, 0x0432, 0x045A,
   0x045F, 0x0455, 0x0437, 0x0448, 0x0452, 0x045B, 0x0447, 0x007F
  };

char32_t const iso_ir_147_table[96] =
  {
   0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
   0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
   0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
   0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
   0x0416, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
   0x0425, 0x0418, 0x0408, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
   0x041F, 0x0409, 0x0420, 0x0421, 0x0422, 0x0423, 0x0412, 0x040A,
   0x040F, 0x0405, 0x0417, 0x0428, 0x0403, 0x040C, 0x0427, 0x005F,
   0x0436, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
   0x0445, 0x0438, 0x0458, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
   0x043F, 0x0459, 0x0440, 0x0441, 0x0442, 0x0443, 0x0432, 0x045A,
   0x045F, 0x0455, 0x0437, 0x0448, 0x0453, 0x045C, 0x0447, 0x007F
  };

char32_t const iso_ir_231_table[96] =
  {
   0x0000, 0x0141, 0x00D8, 0x0110, 0x00DE, 0x00C6, 0x0152, 0x02B9,
   0x00B7, 0x266D, 0x00AE, 0x00B1, 0x01A0, 0x01AF, 0x02BC, 0x0000,
   0x02BB, 0x0142, 0x00F8, 0x0111, 0x00FE, 0x00E6, 0x0153, 0x02BA,
   0x0131, 0x00A3, 0x00F0, 0x0000, 0x01A1, 0x01B0, 0x0000, 0x0000,
   0x00B0, 0x2113, 0x2117, 0x00A9, 0x266F, 0x00BF, 0x00A1, 0x0000,
   0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
   0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
   0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
   0x0309, 0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0306, 0x0307,
   0x0308, 0x030C, 0x030A, 0xFE20, 0xFE21, 0x0315, 0x030B, 0x0310,
   0x0327, 0x0328, 0x0323, 0x0324, 0x0325, 0x0333, 0x0332, 0x0326,
   0x031C, 0x032E, 0xFE22, 0xFE23, 0x0000, 0x0313, 0x0000, 0x0000,
  };

void putu(char32_t u, std::FILE* file) {
  if ((0x300<= u && u < 0x370) || (0x20D0 <= u && u < 0x2100) || (0xFE20 <= u && u < 0xFE30)) {
    std::fprintf(file, "<U+%X>", u);
  } else {
    contra::encoding::put_u8(u, file);
  }
}

void print_def(std::FILE* file, const char* name, char32_t const* table) {
  std::fprintf(file, "%s\n", name);
  std::fprintf(file, "  load SB94(B)\n");
  std::fprintf(file, "  map ");
  for (unsigned char a = '!'; a <= '~'; a++) {
    char32_t u = table[a - 32];
    if (u != a) {
      std::putc(a, file);
      putu(u, file);
    }
  }
  std::fprintf(file, "\n");
}
void print_def_ref(std::FILE* file, const char* name, char32_t const* table, const char* refname, char32_t const* reftable) {
  std::fprintf(file, "%s\n", name);
  std::fprintf(file, "  load %s\n", refname);
  std::fprintf(file, "  map ");
  for (unsigned char a = '!'; a <= '~'; a++) {
    char32_t u = table[a - 32];
    if (u != reftable[a - 32]) {
      std::putc(a, file);
      putu(u, file);
    }
  }
  std::fprintf(file, "\n");
}
void print_def_raw(std::FILE* file, const char* name, char32_t const* table) {
  std::fprintf(file, "%s\n", name);
  std::fprintf(file, "  map ");
  for (unsigned char a = '!'; a <= '~'; a++) {
    char32_t u = table[a - 32];
    if (u) {
      std::putc(a, file);
      putu(u, file);
    }
  }
  std::fprintf(file, "\n");
}

int main() {
  print_def(stdout, "ISO-IR-146", iso_ir_146_table);
  print_def_ref(stdout, "ISO-IR-147", iso_ir_147_table, "ISO-IR-146", iso_ir_146_table);
  print_def_raw(stdout, "ISO-IR-231", iso_ir_231_table);
  return 0;
}