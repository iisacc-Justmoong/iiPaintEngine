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
    if(config.find("iiFileProvider")!=std::string::npos
        || config.find("iiSDKCore")!=std::string::npos
        || config.find("find_dependency(Qt")!=std::string::npos) return 2;
    if(!std::filesystem::exists(root/"third_party/stb/LICENSE")) return 3;
    if(read(root/"LICENSE").find("Apache License")==std::string::npos
        || read(root/"LICENSE").find("Version 2.0")==std::string::npos) return 4;
    if(read(root/"NOTICE").find("Copyright (c) 2026 iisacc")==std::string::npos) return 5;
    if(config.find("set(iiPaintEngine_SPDX_LICENSE \"Apache-2.0\")")==std::string::npos) return 6;
    if(read(root/"docs/BrushTools-LICENSE.txt").find("Apache License")==std::string::npos
        || read(root/"docs/BrushTools-LICENSE.txt").find("Copyright (c) 2026 iisacc")==std::string::npos) return 7;
    if(read(root/"cmake/iiBrushToolsConfig.cmake.in").find("set(iiBrushTools_SPDX_LICENSE \"Apache-2.0\")")==std::string::npos) return 8;
    return 0;
}
