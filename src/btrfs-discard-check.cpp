#include <string>
#include <string_view>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <format>
#include <vector>
#include <nlohmann/json.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

import cxxbtrfs;

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

struct qcow_map {
    bool data;
    bool present;
    bool zero;
    uint64_t start;
    uint64_t length;
    uint64_t offset;
};

class qcow {
public:
    qcow(const char* filename);
    void read(uint64_t offset, span<uint8_t> buf);

    mapping mmap;
    vector<qcow_map> qm;
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

    for (const auto& m : map) {
        if ((bool)m["compressed"])
            throw runtime_error("Cannot handle compressed qcow2 files.");
    }

    return map;
}

qcow::qcow(const char* filename) : mmap(filename) {
    auto map = get_map(filename);

    for (const auto& m : map) {
        auto data = (bool)m.at("data");
        auto present = (bool)m.at("present");
        auto zero = (bool)m.at("zero");
        auto start = (uint64_t)m.at("start");
        auto length = (uint64_t)m.at("length");
        auto offset = present ? (uint64_t)m.at("offset") : 0;

        qm.emplace_back(data, present, zero, start, length, offset);
    }
}

void qcow::read(uint64_t offset, span<uint8_t> buf) {
    auto sp = mmap.get_span();

    while (true) {
        for (const auto& m : qm) {
            if (m.start <= offset && m.start + m.length > offset) {
                auto to_copy = min(buf.size(), m.start + m.length - offset);

                if (m.zero)
                    memset(buf.data(), 0, to_copy);
                else {
                    memcpy(buf.data(), sp.data() + m.offset + offset - m.start,
                           to_copy);
                }

                if (buf.size() == to_copy)
                    return;

                buf = buf.subspan(to_copy);

                break;
            } else if (m.start >= offset + buf.size())
                throw runtime_error("mappings not contiguous");
        }
    }
}

void check_qcow(const char* filename) {
    qcow q(filename);

    btrfs::super_block sb;

    // FIXME - if first superblock not valid, check others

    q.read(btrfs::superblock_addrs[0], span((uint8_t*)&sb, sizeof(sb)));

    if (sb.magic != btrfs::MAGIC)
        throw runtime_error("volume was not btrfs");

    // FIXME - check superblock csum

    // FIXME - read chunk tree
    // FIXME - read dev extents tree
    // FIXME - compare dev extents tree with qcow map

    // FIXME - die if FST flag not set
    // FIXME - read free space tree
    // FIXME - compare FST with qcow map
}

int main() {
    static const char filename[] = "../test.img"; // FIXME - get from args

    try {
        check_qcow(filename);
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
}
