#include <windows.h>
#include <cstdio>
#include <cstring>
#include <memory>

int main(int argc, char** argv) {
  const char* progname = 0 < argc ? argv[0] : "peheader";
  if (!(1 < argc)) {
    std::fprintf(stderr, "usage: %s filename.exe\n", progname);
    return 1;
  }
  //std::FILE* f = std::fopen(argv[1], "r+b");
  std::unique_ptr<std::FILE, int(*)(std::FILE*)> file(std::fopen(argv[1], "r+b"), std::fclose);
  auto f = file.get();
  if (!f) {
    std::fprintf(f, "%s: failed to open the file.", argv[1]);
    return 1;
  }

  IMAGE_DOS_HEADER dosheader;
  std::fread(&dosheader, sizeof(dosheader), 1, f);
  if (dosheader.e_magic != 0x5A4D) {
    std::fprintf(f, "%s: this is not an executable.", argv[1]);
    return 1;
  }
  std::printf("DOS.e_lfanew=#%04X\n", dosheader.e_lfanew);
  std::fflush(f);

  IMAGE_NT_HEADERS32 ntheader;
  std::fseek(f, dosheader.e_lfanew, SEEK_SET);
  std::fread(&ntheader, sizeof(ntheader), 1, f);
  if (ntheader.OptionalHeader.Magic != 0x010B && ntheader.OptionalHeader.Magic != 0x020B) {
    std::fprintf(f, "%s: failed to get NT.OptionalHeader", argv[1]);
    return 1;
  }
  //std::printf("NT.OptionalHeader.Magic=%04x\n", ntheader.OptionalHeader.Magic);
  std::printf("NT.OptionalHeader.Subsystem=%d\n", ntheader.OptionalHeader.Subsystem);
  std::printf("NT.OptionalHeader.MajorSubsystemVersion=%d\n", ntheader.OptionalHeader.MajorSubsystemVersion);
  std::printf("NT.OptionalHeader.MinorSubsystemVersion=%d\n", ntheader.OptionalHeader.MinorSubsystemVersion);

  if (argc >= 3) {
    if (std::strcmp(argv[2], "subsystem=windows") == 0) {
      std::printf("Rewrite Subsystem=2\n");
      std::fseek(f, dosheader.e_lfanew, SEEK_SET);
      ntheader.OptionalHeader.Subsystem = 2;
      std::fwrite(&ntheader, sizeof(ntheader), 1, f);
    } else if (std::strcmp(argv[2], "subsystem=console") == 0) {
      std::printf("Rewrite Subsystem=3\n");
      std::fseek(f, dosheader.e_lfanew, SEEK_SET);
      ntheader.OptionalHeader.Subsystem = 3;
      std::fwrite(&ntheader, sizeof(ntheader), 1, f);
    }
  }

  //std::fclose(f);
  return 0;
}
