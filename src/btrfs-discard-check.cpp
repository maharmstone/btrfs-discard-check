#include <string>
#include <string_view>
#include <memory>
#include <stdexcept>
#include <iostream>

using namespace std;

class pcloser {
public:
    using pointer = FILE*;

    void operator()(FILE* f) {
        pclose(f);
    }
};

static string qemu_img_map(const char* filename) {
    string cmd = "qemu-img map --output json "s + filename;

    char buf[255];
    string ret;
    unique_ptr<FILE, pcloser> pipe{popen(cmd.data(), "r")};

    if (!pipe)
        throw runtime_error("popen failed"); // FIXME - show errno

    while (fgets(buf, sizeof(buf), pipe.get())) {
        ret += buf;
    }

    return ret;
}

int main() {
    static const char filename[] = "../test.img"; // FIXME - get from args

    try {
        auto s = qemu_img_map(filename);
        cout << s << endl;

        // FIXME - parse JSON
        // FIXME - die if any parts are compressed

        // FIXME - mmap
        // FIXME - read superblock
        // FIXME - die if FST flag not set
        // FIXME - read chunk tree
        // FIXME - read dev extents tree
        // FIXME - compare dev extents tree with qcow map

        // FIXME - read free space tree
        // FIXME - compare FST with qcow map
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
}
