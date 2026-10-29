#include "FileProviderBitmapIO.h"
#include <iiFileProviderPresentation.h>
#include <iiFileProviderStorage.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

using iiFileProvider::native::File;
using iiFileProvider::native::Presentation;
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
struct Fixture final {
    std::filesystem::path path;
    Fixture() {
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt=0;attempt<128;++attempt) {
            auto candidate=std::filesystem::current_path() / ("paint-io-"+std::to_string(stamp)+"-"+std::to_string(attempt));
            std::error_code error;
            if (iiFileProvider::native::createDirectory(candidate,error)) { path=std::move(candidate); return; }
            if (error) throw std::system_error(error,"Cannot create test directory");
        }
        throw std::runtime_error("Cannot reserve an exclusive test directory");
    }
    ~Fixture() {
        if (path.empty()) return;
        try { Presentation::cancelPending(path); } catch (...) { return; }
        std::error_code error; std::filesystem::remove_all(path,error);
    }
};
int main() try {
    Fixture fixture;
    auto io=std::make_shared<paint_file::FileProviderBitmapIO>();
    const auto path=fixture.path / u8"한글-그림.png";
    BitmapPaintSession session(io);
    check(session.createFile(path,8,8,"png",0xFF204080U),"create through provider");
    const auto before=File::read(path);
    BitmapFile image(io);
    check(image.open(path) && image.width()==8 && image.height()==8,"read provider bytes");
    check(image.setPixel({2,3},0xFF50AA77U) && image.save(),"atomic replacement");
    check(File::read(path)!=before,"changed image was published");
    const auto after=File::read(path);
    const auto directory=fixture.path/"directory";
    File::createDirectories(directory);
    check(!image.saveAs(directory,"png") && image.filePath()==path,"failed publication preserves current state");
    check(File::read(path)==after,"failed publication preserves original bytes");
    check(Presentation::waitForIdle(std::chrono::seconds(10)),"presentation worker stopped");
    const auto details=Presentation::load(path);
    check(details && details->details.mimeType=="image/png" && details->preview.kind=="image" &&
          !details->preview.bytes.empty(),"provider published actual image details and preview");
    std::cout<<"bitmap provider publication and preview passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
