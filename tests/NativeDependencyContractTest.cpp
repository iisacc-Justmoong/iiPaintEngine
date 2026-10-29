#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <iostream>
namespace {
bool isProviderCompanion(const std::filesystem::path& path)
{
    const auto name = path.filename().string();
    return name == ".iisacc-temporary.lock"
        || (name.starts_with('.') && name.ends_with(".iisacc-preview"))
        || name.ends_with(".iisacc.json");
}
}
int main()
{
    const std::filesystem::path root{IIPAINTENGINE_SOURCE_DIR};
    const std::regex forbiddenInclude{R"(#\s*include\s*[<"](?:Q[A-Z][A-Za-z0-9_/]*|Qt[^">]*|LVRS[^">]*|iiFileProvider[^">]*|iiSDKCore[^">]*|[^">]*QtAdapter)[">])"};
    for (const auto &entry:std::filesystem::recursive_directory_iterator(root/"src")) {
        if(!entry.is_regular_file()) continue;
        if(entry.path().filename()==".DS_Store" || isProviderCompanion(entry.path())) continue;
        const auto extension=entry.path().extension().string();
        if(extension!=".cpp" && extension!=".h" && extension!=".hpp" && entry.path().filename()!="iiPaintEngine") {
            std::cerr << entry.path() << "\n"; return 1;
        }
        std::ifstream f(entry.path()); const std::string s{std::istreambuf_iterator<char>{f},{}};
        if(std::regex_search(s,forbiddenInclude)) { std::cerr<<entry.path()<<"\n"; return 2; }
    }
    for(const auto &dir:{"Example","tests"})
        for(const auto &entry:std::filesystem::recursive_directory_iterator(root/dir)) {
            if(!entry.is_regular_file()) continue;
            const auto relative=entry.path().lexically_relative(root).generic_string();
            if (relative.starts_with("Example/FileProvider/")) continue;
            if(entry.path().filename()==".DS_Store" || isProviderCompanion(entry.path())) continue;
            const auto e=entry.path().extension().string();
            if(e!=".cpp" && e!=".h" && e!=".cmake" && entry.path().filename()!="CMakeLists.txt") { std::cerr << entry.path() << "\n"; return 3; }
        }
    std::ifstream f(root/"CMakeLists.txt"); const std::string s{std::istreambuf_iterator<char>{f},{}};
    if(s.find("find_package(Qt")!=std::string::npos || s.find("AUTOMOC")!=std::string::npos
        || s.find("OBJCXX")!=std::string::npos || s.find("Qt6::")!=std::string::npos
        || s.find("find_package(iiFileProvider")!=std::string::npos || s.find("iiFileProvider::")!=std::string::npos) return 4;
    std::ifstream package(root/"cmake/iiPaintEngineConfig.cmake.in");
    const std::string config{std::istreambuf_iterator<char>{package},{}};
    if (config.find("iiFileProvider")!=std::string::npos || config.find("iiSDKCore")!=std::string::npos) return 5;
    std::ifstream adapter(root/"Example/FileProvider/CMakeLists.txt");
    const std::string application{std::istreambuf_iterator<char>{adapter},{}};
    if (application.find("# iisdk:consumer-application")==std::string::npos) return 6;
    return 0;
}
