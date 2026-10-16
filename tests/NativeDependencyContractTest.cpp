#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <iostream>
int main()
{
    const std::filesystem::path root{IIPAINTENGINE_SOURCE_DIR};
    const std::regex forbiddenInclude{R"(#\s*include\s*[<"](?:Q[A-Z][A-Za-z0-9_/]*|Qt[^">]*|LVRS[^">]*|iiFileProvider(?!Storage\.h|Native\.h)[^">]*|[^">]*QtAdapter)[">])"};
    for (const auto &entry:std::filesystem::recursive_directory_iterator(root/"src")) {
        if(!entry.is_regular_file()) continue;
        if(entry.path().filename()==".DS_Store") continue; // Finder metadata is not source code.
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
            if(entry.path().filename()==".DS_Store") continue;
            const auto e=entry.path().extension().string();
            if(e!=".cpp" && e!=".h" && e!=".cmake") { std::cerr << entry.path() << "\n"; return 3; }
        }
    std::ifstream f(root/"CMakeLists.txt"); const std::string s{std::istreambuf_iterator<char>{f},{}};
    if(s.find("find_package(Qt")!=std::string::npos || s.find("AUTOMOC")!=std::string::npos
        || s.find("OBJCXX")!=std::string::npos || s.find("Qt6::")!=std::string::npos) return 4;
    return 0;
}
