#include <iiPaintEngine>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": " #condition "\n"; return 1; } } while (false)
class MemoryFileIO final : public BitmapFileIO {
public:
    std::map<std::filesystem::path, Bytes> files;
    bool rejectWrite=false, throwWrite=false, throwRead=false;
    std::size_t maximum=0;
    std::expected<Bytes,std::string> read(const std::filesystem::path& path,std::size_t limit) override {
        maximum=limit;
        if (throwRead) throw std::runtime_error("read failure");
        auto it=files.find(path);
        if (it==files.end()) return std::unexpected("not found");
        return it->second;
    }
    std::expected<void,std::string> write(const std::filesystem::path& path,std::span<const unsigned char> bytes) override {
        if (throwWrite) throw std::runtime_error("write failure");
        if (rejectWrite) return std::unexpected("write rejected");
        files[path]=Bytes(bytes.begin(),bytes.end());
        return {};
    }
};
int main() {
    auto io=std::make_shared<MemoryFileIO>();
    BitmapFile missing;
    CHECK(!missing.create("no-port.png",4,4) && !missing.isOpen());
    BitmapFile file(io);
    CHECK(file.create("one.png",4,4,"png",0xFF123456U));
    const auto original=io->files.at("one.png");
    CHECK(!original.empty() && !file.isModified());
    CHECK(file.setPixel({1,1},0xFF99AA77U));
    io->rejectWrite=true;
    CHECK(!file.save() && file.isModified() && io->files.at("one.png")==original);
    CHECK(!file.saveAs("two.png") && file.filePath()=="one.png" && !io->files.contains("two.png"));
    io->rejectWrite=false; io->throwWrite=true;
    CHECK(!file.save() && file.isModified() && io->files.at("one.png")==original);
    io->throwWrite=false;
    CHECK(file.save() && !file.isModified());
    BitmapFile copy(io);
    CHECK(copy.open("one.png") && io->maximum==maxBitmapEncodedBytes);
    CHECK(copy.pixels().pixels==file.pixels().pixels);
    const auto before=copy.pixels().pixels;
    io->throwRead=true;
    CHECK(!copy.open("other.png") && copy.filePath()=="one.png" && copy.pixels().pixels==before);
    io->throwRead=false;
    BitmapPaintSession session(io);
    CHECK(session.createFile("session.png",8,8));
    CHECK(session.openFile("session.png") && session.file().filePath()=="session.png");
    session.closeFile();
    CHECK(session.createFile("next.png",8,8));
    return 0;
}
