#include <string>
#include <string_view>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <format>
#include <nlohmann/json.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

using namespace std;
using json = nlohmann::json;

class pcloser {
public:
    using pointer = FILE*;

    void operator()(FILE* f) {
        pclose(f);
    }
};

class mapping {
public:
    mapping(const char* filename);
    ~mapping();

    span<const uint8_t> get_span() {
        return span((uint8_t*)addr, length);
    }

    void* addr;
    size_t length;
};

mapping::mapping(const char* filename) {
    auto fd = open(filename, O_RDONLY);
    if (fd < 0)
        throw runtime_error("open failed"); // FIXME - show errno

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        throw runtime_error("fstat failed"); // FIXME - errno
    }

    length = st.st_size;

    addr = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        throw runtime_error("mmap failed"); // FIXME - errno
    }

    close(fd);
}

mapping::~mapping() {
    munmap(addr, length);
}

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

static json get_map(const char* filename) {
    auto s = qemu_img_map(filename);

    auto map = json::parse(s);

    if (map.type() != json::value_t::array)
        throw runtime_error("JSON was not an array");

    for (auto& m : map) {
        if ((bool)m["compressed"])
            throw runtime_error("Cannot handle compressed qcow2 files.");
    }

    return map;
}

int main() {
    static const char filename[] = "../test.img"; // FIXME - get from args

    try {
        auto map = get_map(filename);

        mapping m(filename);

        auto sp = m.get_span();

        cout << format("sp = {:x}, {:x}\n", (uintptr_t)sp.data(), sp.size());

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
