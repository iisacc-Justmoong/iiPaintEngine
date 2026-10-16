#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main()
{
    const std::filesystem::path root{IIPAINTENGINE_SOURCE_DIR};
    const auto read=[](const auto &path) { std::ifstream f(path); return std::string{std::istreambuf_iterator<char>{f},{}}; };
    const auto cmake=read(root/"CMakeLists.txt");
    const auto config=read(root/"cmake/iiPaintEngineConfig.cmake.in");
    if(cmake.find("LANGUAGES CXX")==std::string::npos || cmake.find("PUBLIC cxx_std_23")==std::string::npos) return 1;
    if(config.find("find_dependency(iiFileProviderNative 0.6 CONFIG")==std::string::npos
        || config.find("find_dependency(Qt")!=std::string::npos) return 2;
    if(!std::filesystem::exists(root/"third_party/stb/LICENSE")) return 3;
    if(read(root/"LICENSE").find("GNU AFFERO GENERAL PUBLIC LICENSE")==std::string::npos) return 4;
    return 0;
}
