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
import formatted_error;

using namespace std;
using json = nlohmann::json;

#define MAX_STRIPES 2

static bool errors_found = false;

struct chunk : btrfs::chunk {
    btrfs::stripe next_stripes[MAX_STRIPES - 1];
};

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
        throw formatted_error("open failed (errno {})", errno);

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        throw formatted_error("fstat failed (errno {})", errno);
    }

    length = st.st_size;

    addr = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        throw formatted_error("mmap failed (errno {})", errno);
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
        throw formatted_error("popen failed (errno {})", errno);

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
        auto offset = !zero ? (uint64_t)m.at("offset") : 0;

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

static const pair<uint64_t, const chunk&> find_chunk(const map<uint64_t, chunk>& chunks,
                                                     uint64_t address) {
    auto it = chunks.upper_bound(address);

    if (it == chunks.begin())
        throw formatted_error("could not find address {:x} in chunks", address);

    const auto& p = *prev(it);

    if (p.first + p.second.length <= address)
        throw formatted_error("could not find address {:x} in chunks", address);

    return p;
}

template<typename T>
concept walk_func = requires(T t) {
    is_invocable_v<T, const btrfs::key&, span<const uint8_t>>;
    { t(*(btrfs::key*)nullptr, span<const uint8_t>()) } -> same_as<bool>;
};

static bool walk_tree(const qcow& q, const btrfs::super_block& sb, uint64_t address,
                      uint8_t exp_level, uint64_t exp_generation,
                      uint64_t exp_owner, const map<uint64_t, chunk>& chunks,
                      walk_func auto func) {
    auto& [chunk_start, c] = find_chunk(chunks, address);

    switch (btrfs::get_chunk_raid_type(c)) {
        case btrfs::raid_type::RAID0:
        case btrfs::raid_type::RAID10:
        case btrfs::raid_type::RAID5:
        case btrfs::raid_type::RAID6:
            throw formatted_error("unsupported RAID type {}", btrfs::get_chunk_raid_type(c));

        default:
            break;
    }

    vector<uint8_t> v;

    v.resize(sb.nodesize);

    uint64_t phys_address = address - chunk_start + c.stripe[0].offset;

    q.read(phys_address, v);

    auto& h = *(btrfs::header*)v.data();

    if (!btrfs::check_tree_csum(h, sb)) {
        throw formatted_error("csum error while reading tree block at {:x}",
                              address);
    }

    if (h.bytenr != address) {
        throw formatted_error("tree address header mismatch ({:x}, expected {:x})",
                              (uint64_t)h.bytenr, address);
    }

    if (h.level != exp_level) {
        throw formatted_error("tree block at {:x} had level {}, expected {}",
                              address, h.level, exp_level);
    }

    if (h.generation != exp_generation) {
        throw formatted_error("tree block at {:x} had generation {:x}, expected {:x}",
                              address, (uint64_t)h.generation, exp_generation);
    }

    if (h.owner != exp_owner) {
        throw formatted_error("tree block at {:x} had owner {:x}, expected {:x}",
                              address, (uint64_t)h.owner, exp_owner);
    }

    if (h.level > 0) {
        span items((btrfs::key_ptr*)(v.data() + sizeof(btrfs::header)),
                   h.nritems);

        for (const auto& it : items) {
            if (!walk_tree(q, sb, it.blockptr, exp_level - 1, it.generation,
                           exp_owner, chunks, func)) {
                return false;
            }
        }

        return true;
    } else {
        span items((btrfs::item*)(v.data() + sizeof(btrfs::header)), h.nritems);

        for (const auto& it : items) {
            auto sp = span((uint8_t*)v.data() + sizeof(btrfs::header) + it.offset,
                        it.size);

            if (!func(it.key, sp))
                return false;
        }

        return true;
    }
}

template<typename T>
concept find_item_func = is_invocable_v<T, span<const uint8_t>>;

static bool find_item(const qcow& q, const btrfs::super_block& sb, uint64_t address,
                      uint8_t exp_level, uint64_t exp_generation,
                      uint64_t exp_owner, const map<uint64_t, chunk>& chunks,
                      const btrfs::key& search_key, find_item_func auto func) {
    auto& [chunk_start, c] = find_chunk(chunks, address);

    switch (btrfs::get_chunk_raid_type(c)) {
        case btrfs::raid_type::RAID0:
        case btrfs::raid_type::RAID10:
        case btrfs::raid_type::RAID5:
        case btrfs::raid_type::RAID6:
            throw formatted_error("unsupported RAID type {}", btrfs::get_chunk_raid_type(c));

        default:
            break;
    }

    vector<uint8_t> v;

    v.resize(sb.nodesize);

    uint64_t phys_address = address - chunk_start + c.stripe[0].offset;

    q.read(phys_address, v);

    auto& h = *(btrfs::header*)v.data();

    if (!btrfs::check_tree_csum(h, sb)) {
        throw formatted_error("csum error while reading tree block at {:x}",
                              address);
    }

    if (h.bytenr != address) {
        throw formatted_error("tree address header mismatch ({:x}, expected {:x})",
                              (uint64_t)h.bytenr, address);
    }

    if (h.level != exp_level) {
        throw formatted_error("tree block at {:x} had level {}, expected {}",
                              address, h.level, exp_level);
    }

    if (h.generation != exp_generation) {
        throw formatted_error("tree block at {:x} had generation {:x}, expected {:x}",
                              address, (uint64_t)h.generation, exp_generation);
    }

    if (h.owner != exp_owner) {
        throw formatted_error("tree block at {:x} had owner {:x}, expected {:x}",
                              address, (uint64_t)h.owner, exp_owner);
    }

    if (h.level > 0) {
        span items((btrfs::key_ptr*)(v.data() + sizeof(btrfs::header)),
                   h.nritems);

        for (size_t i = 0; i < items.size(); i++) {
            if (items[i].key == search_key)
                return find_item(q, sb, items[i].blockptr, exp_level - 1,
                                 items[i].generation, exp_owner, chunks,
                                 search_key, func);

            if (items[i].key > search_key) {
                if (i == 0)
                    return false;

                return find_item(q, sb, items[i - 1].blockptr, exp_level - 1,
                                 items[i - 1].generation, exp_owner, chunks,
                                 search_key, func);
            }
        }

        return find_item(q, sb, items[items.size() - 1].blockptr, exp_level - 1,
                         items[items.size() - 1].generation, exp_owner, chunks,
                         search_key, func);
    } else {
        span items((btrfs::item*)(v.data() + sizeof(btrfs::header)), h.nritems);

        for (const auto& it : items) {
            if (it.key == search_key) {
                func(span((uint8_t*)v.data() + sizeof(btrfs::header) + it.offset,
                          it.size));

                return true;
            } else if (it.key > search_key)
                return false;
        }

        return false;
    }
}

static map<uint64_t, chunk> load_chunks(const qcow& q, const btrfs::super_block& sb) {
    map<uint64_t, chunk> sys_chunks, chunks;

    auto sys_array = span(sb.sys_chunk_array.data(), sb.sys_chunk_array_size);

    while (!sys_array.empty()) {
        if (sys_array.size() < sizeof(btrfs::key))
            throw runtime_error("sys array truncated");

        auto& k = *(btrfs::key*)sys_array.data();

        if (k.type != btrfs::key_type::CHUNK_ITEM)
            throw formatted_error("unexpected key type {} in sys array", k.type);

        sys_array = sys_array.subspan(sizeof(btrfs::key));

        if (sys_array.size() < offsetof(btrfs::chunk, stripe))
            throw runtime_error("sys array truncated");

        auto& c = *(chunk*)sys_array.data();

        if (sys_array.size() < offsetof(btrfs::chunk, stripe) + (c.num_stripes * sizeof(btrfs::stripe)))
            throw runtime_error("sys array truncated");

        if (c.num_stripes > MAX_STRIPES) {
            throw formatted_error("chunk num_stripes is {}, maximum supported is {}",
                                  (uint16_t)c.num_stripes, MAX_STRIPES);
        }

        sys_array = sys_array.subspan(offsetof(btrfs::chunk, stripe) + (c.num_stripes * sizeof(btrfs::stripe)));

        sys_chunks.insert(make_pair((uint64_t)k.offset, c));
    }

    walk_tree(q, sb, sb.chunk_root, sb.chunk_root_level, sb.chunk_root_generation,
              btrfs::CHUNK_TREE_OBJECTID, sys_chunks, [&chunks](const btrfs::key& k, span<const uint8_t> sp) {
        if (k.type != btrfs::key_type::CHUNK_ITEM || k.objectid != btrfs::FIRST_CHUNK_TREE_OBJECTID)
            return true;

        if (sp.size() < offsetof(btrfs::chunk, stripe)) {
            throw formatted_error("CHUNK_ITEM truncated ({} bytes, expected at least {})",
                                  sp.size(), offsetof(btrfs::chunk, stripe));
        }

        auto& c = *(chunk*)sp.data();

        if (sp.size() < offsetof(btrfs::chunk, stripe) + (c.num_stripes * sizeof(btrfs::stripe))) {
            throw formatted_error("CHUNK_ITEM truncated ({} bytes, expected {})",
                                  sp.size(), offsetof(btrfs::chunk, stripe) + (c.num_stripes * sizeof(btrfs::stripe)));
        }

        if (c.num_stripes > MAX_STRIPES) {
            throw formatted_error("chunk num_stripes is {}, maximum supported is {}",
                                  (uint16_t)c.num_stripes, MAX_STRIPES);
        }

        chunks.insert(make_pair((uint64_t)k.offset, c));

        return true;
    });

    return chunks;
}

enum class btrfs_alloc {
    unallocated,
    superblock,
    chunk,
    chunk_used,
    chunk_free
};

template<>
struct std::formatter<enum btrfs_alloc> {
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin();

        if (it != ctx.end() && *it != '}')
            throw format_error("invalid format");

        return it;
    }

    template<typename format_context>
    auto format(enum btrfs_alloc t, format_context& ctx) const {
        switch (t) {
            case btrfs_alloc::unallocated:
                return format_to(ctx.out(), "unallocated");
            case btrfs_alloc::superblock:
                return format_to(ctx.out(), "superblock");
            case btrfs_alloc::chunk:
                return format_to(ctx.out(), "chunk");
            case btrfs_alloc::chunk_used:
                return format_to(ctx.out(), "chunk_used");
            case btrfs_alloc::chunk_free:
                return format_to(ctx.out(), "chunk_free");
            default:
                return format_to(ctx.out(), "{}", (unsigned int)t);
        }
    }
};

struct btrfs_extent {
    uint64_t offset;
    uint64_t length;
    enum btrfs_alloc alloc;
    uint64_t address;
};

struct qcow_extent {
    uint64_t offset;
    uint64_t length;
    bool alloc;
};

struct extent2 {
    uint64_t offset;
    uint64_t length;
    bool qcow_alloc;
    enum btrfs_alloc btrfs_alloc;
    uint64_t address;
};

static void carve_out_superblocks(vector<btrfs_extent>& extents) {
    vector<btrfs_extent> ret;

    for (const auto& e : extents) {
        bool superblock_added = false;

        for (auto addr : btrfs::superblock_addrs) {
            if (addr >= e.offset && addr + sizeof(btrfs::super_block) <= e.offset + e.length) {
                if (addr > e.offset) {
                    ret.emplace_back(e.offset, addr - e.offset, e.alloc,
                                     e.address);
                }

                ret.emplace_back(addr, sizeof(btrfs::super_block),
                                 btrfs_alloc::superblock,
                                 e.alloc == btrfs_alloc::unallocated ? 0 : e.address + addr - e.offset);

                if (e.offset + e.length > addr + sizeof(btrfs::super_block) ) {
                    ret.emplace_back(addr + sizeof(btrfs::super_block),
                                     e.offset + e.length - addr - sizeof(btrfs::super_block),
                                     e.alloc, e.address + addr + sizeof(btrfs::super_block) - e.offset);
                }

                superblock_added = true;
                break;
            }
        }

        if (!superblock_added)
            ret.push_back(move(e));
    }

    extents.swap(ret);
}

static map<uint64_t, vector<extent2>> check_dev_tree(const qcow& q,
                                                     const map<uint64_t, chunk>& chunks,
                                                     const btrfs::super_block& sb) {
    uint64_t dev_root, dev_generation;
    uint8_t dev_level;

    static const btrfs::key search_key = { btrfs::DEV_TREE_OBJECTID, btrfs::key_type::ROOT_ITEM, 0 };

    if (!find_item(q, sb, sb.root, sb.root_level, sb.generation, btrfs::ROOT_TREE_OBJECTID,
                   chunks, search_key, [&dev_root, &dev_level, &dev_generation](span<const uint8_t> sp) {
        if (sp.size() < sizeof(btrfs::root_item)) {
            throw formatted_error("ROOT_ITEM truncated ({} bytes, expected {})",
                                  sp.size(), sizeof(btrfs::root_item));
        }

        auto& ri = *(btrfs::root_item*)sp.data();

        dev_root = (uint64_t)ri.bytenr;
        dev_level = ri.level;
        dev_generation = ri.generation;
    }))
        throw runtime_error("ROOT_ITEM for dev tree not found");

    vector<btrfs_extent> extents;
    vector<qcow_extent> qcow_extents;

    optional<uint64_t> last_end;
    walk_tree(q, sb, dev_root, dev_level, dev_generation, btrfs::DEV_TREE_OBJECTID,
              chunks, [&extents, &last_end](const btrfs::key& k, span<const uint8_t> sp) {
        if (k.type != btrfs::key_type::DEV_EXTENT || k.objectid != 1)
            return true;

        if (sp.size() < sizeof(btrfs::dev_extent)) {
            throw formatted_error("DEV_EXTENT truncated ({} bytes, expected {})",
                                  sp.size(), sizeof(btrfs::dev_extent));
        }

        auto& de = *(btrfs::dev_extent*)sp.data();
        auto length = de.length;

        if (!last_end.has_value()) {
            if (k.offset != 0)
                extents.emplace_back(0, k.offset, btrfs_alloc::unallocated, 0);
        } else if (k.offset > *last_end)
            extents.emplace_back(*last_end, k.offset - *last_end,
                                 btrfs_alloc::unallocated,  0);

        extents.emplace_back(k.offset, length, btrfs_alloc::chunk,
                             (uint64_t)de.chunk_offset);

        last_end = k.offset + length;

        return true;
    });

    auto size = q.qm.back().start + q.qm.back().length;

    if (!last_end.has_value())
        extents.emplace_back(0, size, btrfs_alloc::unallocated, 0);
    else if (*last_end < size) {
        extents.emplace_back(*last_end, size - *last_end,
                             btrfs_alloc::unallocated, 0);
    }

    for (const auto& m : q.qm) {
        if (!qcow_extents.empty() &&
            qcow_extents.back().offset + qcow_extents.back().length == m.start &&
            !!qcow_extents.back().alloc == !m.zero) {
            qcow_extents.back().length += m.length;
        } else
            qcow_extents.emplace_back(m.start, m.length, !m.zero);
    }

    carve_out_superblocks(extents);

    vector<extent2> merged;

    size_t i = 0, j = 0;
    while (i < extents.size() && j < qcow_extents.size()) {
        auto& be = extents[i];
        auto& qe = qcow_extents[j];

        if (be.length == qe.length) {
            merged.emplace_back(be.offset, be.length, qe.alloc, be.alloc,
                                be.address);
            i++;
            j++;
        } else if (be.length < qe.length) {
            merged.emplace_back(be.offset, be.length, qe.alloc, be.alloc,
                                be.address);
            qe.offset += be.length;
            qe.length -= be.length;
            i++;
        } else {
            merged.emplace_back(be.offset, qe.length, qe.alloc, be.alloc,
                                be.address);
            be.offset += qe.length;
            be.length -= qe.length;
            be.address += qe.length;
            j++;
        }
    }

    map<uint64_t, vector<extent2>> by_chunk;

    for (auto& m : merged) {
        uint64_t chunk_address;

        if (m.btrfs_alloc == btrfs_alloc::chunk || (m.btrfs_alloc == btrfs_alloc::superblock && m.address != 0)) {
            auto it = chunks.upper_bound(m.address);

            chunk_address = prev(it)->first;
        } else
            chunk_address = 0;

        by_chunk[chunk_address].push_back(move(m));
    }

    for (const auto& bc : by_chunk) {
        if (bc.first == 0) {
            for (const auto& m : bc.second) {
                if (m.btrfs_alloc == btrfs_alloc::superblock && !m.qcow_alloc) {
                    cerr << format("superblock at {:x} not allocated", m.offset) << endl;
                    errors_found = true;
                } else if (m.btrfs_alloc == btrfs_alloc::unallocated && m.qcow_alloc) {
                    if (m.offset + m.length <= btrfs::DEVICE_RANGE_RESERVED)
                        continue;

                    uint64_t offset, length;

                    if (m.offset < btrfs::DEVICE_RANGE_RESERVED) {
                        offset = btrfs::DEVICE_RANGE_RESERVED;
                        length = m.offset + m.length - btrfs::DEVICE_RANGE_RESERVED;
                    } else {
                        offset = m.offset;
                        length = m.length;
                    }

                    cerr << format("qcow range {:x}, {:x} allocated but not part of any btrfs chunk",
                                   offset, length) << endl;
                    errors_found = true;
                }
            }
        }
    }

    return by_chunk;
}

struct space_entry {
    uint64_t address;
    uint64_t length;
    bool alloc;
};

struct space_entry2 {
    uint64_t log_address;
    uint64_t phys_address;
    uint64_t length;
    bool alloc;
};

static map<uint64_t, vector<space_entry2>> read_fst(const qcow& q,
                                                    const map<uint64_t, chunk>& chunks,
                                                    const btrfs::super_block& sb) {
    uint64_t fst_root, fst_generation;
    uint8_t fst_level;

    static const btrfs::key search_key = { btrfs::FREE_SPACE_TREE_OBJECTID, btrfs::key_type::ROOT_ITEM, 0 };

    if (!find_item(q, sb, sb.root, sb.root_level, sb.generation, btrfs::ROOT_TREE_OBJECTID,
                   chunks, search_key, [&fst_root, &fst_level, &fst_generation](span<const uint8_t> sp) {
        if (sp.size() < sizeof(btrfs::root_item)) {
            throw formatted_error("ROOT_ITEM truncated ({} bytes, expected {})",
                                  sp.size(), sizeof(btrfs::root_item));
        }

        auto& ri = *(btrfs::root_item*)sp.data();

        fst_root = (uint64_t)ri.bytenr;
        fst_level = ri.level;
        fst_generation = ri.generation;
    }))
        throw runtime_error("ROOT_ITEM for free space tree not found");

    vector<pair<uint64_t, uint64_t>> free_space;

    walk_tree(q, sb, fst_root, fst_level, fst_generation, btrfs::FREE_SPACE_TREE_OBJECTID,
              chunks, [&free_space, &sb](const btrfs::key& k, span<const uint8_t> sp) {
        if (k.type == btrfs::key_type::FREE_SPACE_EXTENT)
            free_space.emplace_back(k.objectid, k.offset);
        else if (k.type == btrfs::key_type::FREE_SPACE_BITMAP) {
            vector<pair<uint64_t, uint64_t>> bmp;

            unsigned int pos = 0;

            while (!sp.empty()) {
                auto num = sp[0];

                for (unsigned int i = 0; i < 8; i++) {
                    if (num & 1) {
                        if (!bmp.empty() && bmp.back().first + bmp.back().second == pos)
                            bmp.back().second++;
                        else
                            bmp.emplace_back(pos, 1);
                    }

                    pos++;
                    num >>= 1;
                }

                sp = sp.subspan(1);
            }

            for (const auto& b : bmp) {
                free_space.emplace_back(k.objectid + (b.first * sb.sectorsize),
                                        b.second * sb.sectorsize);
            }
        }

        return true;
    });

    map<uint64_t, vector<space_entry>> space;

    for (auto& f : free_space) {
        uint64_t chunk_address;

        auto it = chunks.upper_bound(f.first);

        if (it == chunks.begin()) {
            cerr << format("free space entry {:x}, {:x} not part of any chunk",
                           f.first, f.second) << endl;
            errors_found = true;
            continue;
        }

        chunk_address = prev(it)->first;

        if (!space.contains(chunk_address)) {
            if (f.first > chunk_address) {
                space[chunk_address].emplace_back(chunk_address,
                            f.first - chunk_address, true);
            }
        } else {
            const auto& l = space[chunk_address].back();

            if (f.first > l.address + l.length) {
                space[chunk_address].emplace_back(l.address + l.length,
                            f.first - l.address - l.length, true);
            }
        }

        space[chunk_address].emplace_back(f.first, f.second, false);
    }

    // handle fully-allocated chunks

    for (const auto& c : chunks) {
        if (!space.contains(c.first))
            space[c.first].emplace_back(c.first, c.second.length, true);
    }

    for (auto& bc : space) {
        auto& c = chunks.at(bc.first);

        if (bc.second.empty())
            bc.second.emplace_back(bc.first, c.length, false);
        else {
            const auto& l = bc.second.back();

            if (l.address + l.length < bc.first + c.length) {
                bc.second.emplace_back(l.address + l.length,
                                       bc.first + c.length - l.address - l.length,
                                       false);
            }
        }
    }

    map<uint64_t, vector<space_entry2>> space2;

    for (auto& bc : space) {
        auto& c = chunks.at(bc.first);

        vector<const btrfs::stripe*> stripes;

        for (unsigned int i = 0; i < c.num_stripes; i++) {
            stripes.push_back(&c.stripe[i]);
        }

        sort(stripes.begin(), stripes.end(), [](const auto& a, const auto& b) {
            return a->offset < b->offset;
        });

        for (const auto& s : stripes) {
            for (const auto& f : bc.second) {
                uint64_t phys = f.address - bc.first + s->offset;

                space2[bc.first].emplace_back(f.address, phys, f.length, f.alloc);
            }
        }
    }

    return space2;
}

static void do_merge2(uint64_t chunk_address, vector<extent2>& dev_extents,
                      vector<space_entry2>& space) {
    (void)chunk_address;

#if 0
    cout << format("chunk {:x}:", chunk_address) << endl;

    for (const auto& m : dev_extents) {
        cout << format("physical address {:x}, length {:x}, qcow_alloc {}, logical address {:x}\n",
                       m.offset, m.length, m.qcow_alloc, m.address);
    }

    for (const auto& f : space) {
        cout << format("space: physical address {:x}, length {:x}, logical address {:x}, alloc {}",
                       f.phys_address, f.length, f.log_address, f.alloc) << endl;
    }
#endif

    vector<extent2> merged;

    size_t i = 0, j = 0;
    while (i < dev_extents.size() && j < space.size()) {
        auto& d = dev_extents[i];
        auto& s = space[j];

        enum btrfs_alloc alloc;

        if (d.btrfs_alloc == btrfs_alloc::superblock)
            alloc = btrfs_alloc::superblock;
        else if (s.alloc)
            alloc = btrfs_alloc::chunk_used;
        else
            alloc = btrfs_alloc::chunk_free;

        assert(d.offset == s.phys_address);

        if (d.length == s.length) {
            merged.emplace_back(d.offset, d.length, d.qcow_alloc, alloc,
                                d.address);
            i++;
            j++;
        } else if (d.length < s.length) {
            merged.emplace_back(d.offset, d.length, d.qcow_alloc, alloc,
                                d.address);
            s.phys_address += d.length;
            s.log_address += d.length;
            s.length -= d.length;
            i++;
        } else {
            merged.emplace_back(d.offset, s.length, d.qcow_alloc, alloc,
                                d.address);
            d.offset += s.length;
            d.address += s.length;
            d.length -= s.length;
            j++;
        }
    }

#if 0
    for (const auto& f : merged) {
        cout << format("merged: physical address {:x}, length {:x}, logical address {:x}, qcow_alloc {}, btrfs_alloc {}",
                       f.offset, f.length, f.address, f.qcow_alloc,
                       f.btrfs_alloc) << endl;
    }
#endif

    for (const auto& f : merged) {
        if (f.qcow_alloc && f.btrfs_alloc == btrfs_alloc::chunk_free) {
            cerr << format("qcow range {:x}, {:x} allocated (address {:x}) but is free space",
                           f.offset, f.length, f.address) << endl;
            errors_found = true;
        } else if (!f.qcow_alloc && f.btrfs_alloc == btrfs_alloc::chunk_used) {
            cerr << format("qcow range {:x}, {:x} discarded (address {:x}) but is allocated",
                           f.offset, f.length, f.address) << endl;
            errors_found = true;
        }
    }
}

static void do_merge(map<uint64_t, vector<extent2>>& dev_extents,
                     map<uint64_t, vector<space_entry2>>& space) {
    for (auto& d : dev_extents) {
        if (d.first == 0)
            continue;

        do_merge2(d.first, d.second, space.at(d.first));
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

    if (!btrfs::check_superblock_csum(sb))
        throw runtime_error("superblock csum mismatch");

    auto chunks = load_chunks(q, sb);

    auto dev_extents = check_dev_tree(q, chunks, sb);

    if (!(sb.compat_ro_flags & btrfs::FEATURE_COMPAT_RO_FREE_SPACE_TREE)) {
        cerr << "not analysing free space as filesystem is not using free space tree" << endl;
        return;
    }

    auto space = read_fst(q, chunks, sb);

    do_merge(dev_extents, space);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: btrfs-dischard-check <qcow-image>" << endl;
        return 1;
    }

    try {
        check_qcow(argv[1]);
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return 1;
    }

    return errors_found ? 1 : 0;
}
