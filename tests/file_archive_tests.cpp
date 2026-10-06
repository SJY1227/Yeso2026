#include "../firmware/RoutineDevice/src/platform/FileArchive.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
using namespace routine::platform;
int main(){
  const auto dir=std::filesystem::path("build/archive-tests");std::filesystem::create_directories(dir);
  const auto final=(dir/"history-1.bin").string(),temp=(dir/"history-1.tmp").string();
  std::filesystem::remove(final);std::filesystem::remove(temp);
  const auto sync=[](FILE*){return true;};const auto fail=[](FILE*){return false;};
  std::vector<uint8_t> data(1500);for(size_t i=0;i<data.size();++i)data[i]=uint8_t(i);
  assert(writeArchive(final.c_str(),temp.c_str(),data.data(),data.size(),fail)==ArchiveResult::IoError);
  assert(!std::filesystem::exists(final));
  std::ofstream(temp,std::ios::binary)<<"interrupted candidate";
  assert(writeArchive(final.c_str(),temp.c_str(),data.data(),data.size(),sync)==ArchiveResult::Saved);
  assert(!std::filesystem::exists(temp));
  assert(writeArchive(final.c_str(),temp.c_str(),data.data(),data.size(),fail)==ArchiveResult::Existing);
  data[0]^=1;assert(writeArchive(final.c_str(),temp.c_str(),data.data(),data.size(),sync)==ArchiveResult::Conflict);
  data[0]^=1;assert(writeArchive(final.c_str(),temp.c_str(),data.data(),data.size(),sync)==ArchiveResult::Existing);
  std::ofstream(final,std::ios::binary|std::ios::trunc)<<"damaged final";
  assert(writeArchive(final.c_str(),temp.c_str(),data.data(),data.size(),sync)==ArchiveResult::Conflict);
  assert(std::filesystem::file_size(final)==13); // Evidence is never silently overwritten.
  uint64_t generation=9;assert(archiveGeneration("history-18446744073709551615.bin",generation)&&generation==UINT64_MAX);
  for(const auto* bad:{"history-0.bin","history-01.bin","history--1.bin","history-18446744073709551616.bin","history-1.tmp","../history-1.bin","history-1.bin.extra"})assert(!archiveGeneration(bad,generation));
  std::filesystem::remove(final);std::filesystem::remove(temp);std::filesystem::remove(dir);
  std::puts("PASS: immutable archive, interrupted candidate retry, sync failure, conflicting final preservation and bounded archive names");
}
