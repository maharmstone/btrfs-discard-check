#include <string>
#include <string_view>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <format>
#include <vector>
#include <map>
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

    span<const uint8_t> get_span() const {
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
    void read(uint64_t offset, span<uint8_t> buf) const;

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

void qcow::read(uint64_t offset, span<uint8_t> buf) const {
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

static const pair<uint64_t, const btrfs::chunk&> find_chunk(const map<uint64_t, btrfs::chunk>& chunks,
                                                            uint64_t address) {
    // FIXME - can we use std::map's lower_bound or upper_bound for this?

    for (auto& p : chunks) {
        if (p.first > address)
            throw runtime_error("could not find address in chunks"); // FIXME - include address

        auto& c = p.second;

        if (p.first + c.length <= address)
            continue;

        return p;
    }

    throw runtime_error("could not find address in chunks"); // FIXME - include address
}

template<typename T>
concept walk_func = requires(T t) {
    is_invocable_v<T, const btrfs::key&, span<const uint8_t>>;
    { t(*(btrfs::key*)nullptr, span<const uint8_t>()) } -> same_as<bool>;
};

static void walk_tree(const qcow& q, uint32_t node_size, uint64_t address,
                      const map<uint64_t, btrfs::chunk>& chunks,
                      walk_func auto func) {
    auto& [chunk_start, c] = find_chunk(chunks, address);

    switch (btrfs::get_chunk_raid_type(c)) {
        case btrfs::raid_type::RAID0:
        case btrfs::raid_type::RAID10:
        case btrfs::raid_type::RAID5:
        case btrfs::raid_type::RAID6:
            throw runtime_error("unsupported RAID type"); // FIXME - include name

        default:
            break;
    }

    vector<uint8_t> v;

    v.resize(node_size);

    uint64_t phys_address = address - chunk_start + c.stripe[0].offset;

    q.read(phys_address, v);

    auto& h = *(btrfs::header*)v.data();

    // FIXME - check csum

    if (h.bytenr != address)
        throw runtime_error("tree address header mismatch"); // FIXME - include numbers

    // FIXME - check generation
    // FIXME - check owner
    // FIXME - check level

    if (h.level > 0)
        throw runtime_error("FIXME - internal nodes"); // FIXME

    span items((btrfs::item*)(v.data() + sizeof(btrfs::header)), h.nritems);

    for (const auto& it : items) {
        auto sp = span((uint8_t*)v.data() + sizeof(btrfs::header) + it.offset,
                       it.size);

        if (!func(it.key, sp))
            break;
    }
}

static map<uint64_t, btrfs::chunk> load_chunks(const qcow& q,
                                               const btrfs::super_block& sb) {
    map<uint64_t, btrfs::chunk> sys_chunks, chunks;

    auto sys_array = span(sb.sys_chunk_array.data(), sb.sys_chunk_array_size);

    while (!sys_array.empty()) {
        if (sys_array.size() < sizeof(btrfs::key))
            throw runtime_error("sys array truncated"); // FIXME - include byte counts

        auto& k = *(btrfs::key*)sys_array.data();

        if (k.type != btrfs::key_type::CHUNK_ITEM)
            throw runtime_error("unexpected key type in sys array"); // FIXME - include number

        sys_array = sys_array.subspan(sizeof(btrfs::key));

        if (sys_array.size() < offsetof(btrfs::chunk, stripe))
            throw runtime_error("sys array truncated"); // FIXME - include byte counts

        auto& c = *(btrfs::chunk*)sys_array.data();

        if (sys_array.size() < offsetof(btrfs::chunk, stripe) + (c.num_stripes * sizeof(btrfs::stripe)))
            throw runtime_error("sys array truncated"); // FIXME - include byte counts

        sys_array = sys_array.subspan(offsetof(btrfs::chunk, stripe) + (c.num_stripes * sizeof(btrfs::stripe)));

        // FIXME - can we safely add a variable-length chunk to the map?

        sys_chunks.insert(make_pair((uint64_t)k.offset, c));
    }

    walk_tree(q, sb.nodesize, sb.chunk_root, sys_chunks, [&chunks](const btrfs::key& k, span<const uint8_t> sp) {
        if (k.type != btrfs::key_type::CHUNK_ITEM || k.objectid != btrfs::FIRST_CHUNK_TREE_OBJECTID)
            return true;

        if (sp.size() < offsetof(btrfs::chunk, stripe))
            throw runtime_error("CHUNK_ITEM truncated"); // FIXME - include offset and byte counts

        auto& c = *(btrfs::chunk*)sp.data();

        if (sp.size() < offsetof(btrfs::chunk, stripe) + (c.num_stripes * sizeof(btrfs::stripe)))
            throw runtime_error("CHUNK_ITEM truncated"); // FIXME - include offset and byte counts

        chunks.insert(make_pair((uint64_t)k.offset, c));

        return true;
    });

    return chunks;
}

struct extent {
    uint64_t offset;
    uint64_t length;
    bool alloc;
};

struct extent2 {
    uint64_t offset;
    uint64_t length;
    bool qcow_alloc;
    bool btrfs_alloc;
};

static void check_dev_tree(const qcow& q, const map<uint64_t, btrfs::chunk>& chunks,
                           uint64_t root_tree_root, uint32_t node_size) {
    optional<uint64_t> dev_root;

    static const btrfs::key search_key = { btrfs::DEV_TREE_OBJECTID, btrfs::key_type::ROOT_ITEM, 0 };

    // FIXME - search tree rather than walking

    walk_tree(q, node_size, root_tree_root, chunks, [&dev_root](const btrfs::key& k, span<const uint8_t> sp) {
        if (k > search_key)
            throw runtime_error("ROOT_ITEM for dev tree not found");

        if (k < search_key)
            return true;

        if (sp.size() < sizeof(btrfs::root_item))
            throw runtime_error("ROOT_ITEM truncated"); // FIXME - include byte counts

        auto& ri = *(btrfs::root_item*)sp.data();

        dev_root = (uint64_t)ri.bytenr;

        return false;
    });

    if (!dev_root.has_value())
        throw runtime_error("ROOT_ITEM for dev tree not found");

    vector<::extent> extents, qcow_extents;

    optional<uint64_t> last_end;
    walk_tree(q, node_size, *dev_root, chunks, [&extents, &last_end](const btrfs::key& k, span<const uint8_t> sp) {
        if (k.type != btrfs::key_type::DEV_EXTENT || k.objectid != 1)
            return true;

        if (sp.size() < sizeof(btrfs::dev_extent))
            throw runtime_error("DEV_EXTENT truncated"); // FIXME - include byte counts

        auto& de = *(btrfs::dev_extent*)sp.data();
        auto length = de.length;

        if (!last_end.has_value()) {
            if (k.offset != 0)
                extents.emplace_back(0, k.offset, false);
        } else if (k.offset > *last_end)
            extents.emplace_back(*last_end, k.offset - *last_end, false);

        if (!extents.empty() && extents.back().offset + extents.back().length == k.offset && extents.back().alloc)
            extents.back().length += length;
        else
            extents.emplace_back(k.offset, length, true);

        last_end = k.offset + length;

        return true;
    });

    auto size = q.qm.back().start + q.qm.back().length;

    if (!last_end.has_value())
        extents.emplace_back(0, size, false);
    else if (*last_end < size)
        extents.emplace_back(*last_end, size - *last_end, false);

    for (const auto& m : q.qm) {
        if (!qcow_extents.empty() &&
            qcow_extents.back().offset + qcow_extents.back().length == m.start &&
            !!qcow_extents.back().alloc == !m.zero) {
            qcow_extents.back().length += m.length;
        } else
            qcow_extents.emplace_back(m.start, m.length, !m.zero);
    }

    vector<extent2> merged;

    size_t i = 0, j = 0;
    while (i < extents.size() && j < qcow_extents.size()) {
        auto& be = extents[i];
        auto& qe = qcow_extents[j];

        if (be.length == qe.length) {
            merged.emplace_back(be.offset, be.length, qe.alloc, be.alloc);
            i++;
            j++;
        } else if (be.length < qe.length) {
            merged.emplace_back(be.offset, be.length, qe.alloc, be.alloc);
            qe.offset += be.length;
            qe.length -= be.length;
            i++;
        } else {
            merged.emplace_back(be.offset, qe.length, qe.alloc, be.alloc);
            be.offset += qe.length;
            be.length -= qe.length;
            j++;
        }
    }

    for (const auto& m : merged) {
        cout << format("{:x}, {:x}, {}, {}\n", m.offset, m.length, m.qcow_alloc,
                       m.btrfs_alloc);
    }

    // FIXME - superblocks!

    for (const auto& m : merged) {
        if (m.qcow_alloc && !m.btrfs_alloc)
            cerr << format("qcow range {:x}, {:x} allocated but not part of any btrfs chunk",
                           m.offset, m.length) << endl;
    }
}

static void check_qcow(const char* filename) {
    qcow q(filename);

    btrfs::super_block sb;

    // FIXME - if first superblock not valid, check others

    q.read(btrfs::superblock_addrs[0], span((uint8_t*)&sb, sizeof(sb)));

    if (sb.magic != btrfs::MAGIC)
        throw runtime_error("volume was not btrfs");

    if (sb.num_devices != 1)
        throw runtime_error("multi-device filesystems not supported");

    // FIXME - check superblock csum

    auto chunks = load_chunks(q, sb);

    check_dev_tree(q, chunks, sb.root, sb.nodesize);

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
