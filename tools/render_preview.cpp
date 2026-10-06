#include "../tests/LoadAssets.h"
#include "../firmware/RoutineDevice/src/presentation/HomeRenderer.h"
#include "../firmware/RoutineDevice/src/diagnostics/HardwareCheck.h"
#include <fstream>
#include <string>
#include <vector>
int main(int argc,char** argv) {
  loadTestAssets();
  if (argc!=2) return 2;
  std::vector<uint16_t> pixels(240*320);
  for (int i=0;i<2;++i) {
    routine::ui::HomeViewModel state = routine::diagnostics::HardwareCheck().view();
    state.theme=i==0?routine::ui::Theme::Pink:routine::ui::Theme::Dark;
    routine::ui::renderHome(pixels.data(),state);
    std::ofstream out(std::string(argv[1])+(i==0?"/pink-home.ppm":"/dark-home.ppm"),std::ios::binary);
    if (!out) return 3;
    out << "P6\n240 320\n255\n";
    for (uint16_t c:pixels) {
      const unsigned char rgb[3]={static_cast<unsigned char>(((c>>11)*255+15)/31),static_cast<unsigned char>((((c>>5)&63)*255+31)/63),static_cast<unsigned char>(((c&31)*255+15)/31)};
      out.write(reinterpret_cast<const char*>(rgb),3);
    }
  }
}
