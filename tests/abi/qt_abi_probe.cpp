#include <cstddef>
#include <iostream>
#include <string_view>

struct ByteArrayView {
    std::ptrdiff_t size;
    const char* data;
};

struct QtString {
    void* header;
    const char16_t* characters;
    std::ptrdiff_t size;
};

static_assert(sizeof(QtString) == 24);

extern "C" QtString QtFromUtf8(ByteArrayView view) asm(
    "_ZN2QT7QString8fromUtf8ENS_14QByteArrayViewE");

int main(int argc, char** argv) {
    constexpr std::string_view source = "Xrefs graph";
    const QtString result =
        QtFromUtf8(ByteArrayView{static_cast<std::ptrdiff_t>(source.size()), source.data()});
    const std::u16string_view expected =
        argc > 1 && std::string_view(argv[1]) == "--expect-translated" ? u"交叉引用图"
                                                                       : u"Xrefs graph";
    if (!result.characters || result.size != static_cast<std::ptrdiff_t>(expected.size()) ||
        std::u16string_view(result.characters, static_cast<std::size_t>(result.size)) != expected) {
        std::cerr << "Qt QString ABI/preload translation mismatch\n";
        return 1;
    }
    return 0;
}
