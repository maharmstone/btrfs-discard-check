module;

#include <stdint.h>
#include <array>
#include <format>

export module cxxbtrfs;

using namespace std;

static const uint32_t crctable[] = {
    0x00000000, 0xf26b8303, 0xe13b70f7, 0x1350f3f4, 0xc79a971f, 0x35f1141c, 0x26a1e7e8, 0xd4ca64eb,
    0x8ad958cf, 0x78b2dbcc, 0x6be22838, 0x9989ab3b, 0x4d43cfd0, 0xbf284cd3, 0xac78bf27, 0x5e133c24,
    0x105ec76f, 0xe235446c, 0xf165b798, 0x030e349b, 0xd7c45070, 0x25afd373, 0x36ff2087, 0xc494a384,
    0x9a879fa0, 0x68ec1ca3, 0x7bbcef57, 0x89d76c54, 0x5d1d08bf, 0xaf768bbc, 0xbc267848, 0x4e4dfb4b,
    0x20bd8ede, 0xd2d60ddd, 0xc186fe29, 0x33ed7d2a, 0xe72719c1, 0x154c9ac2, 0x061c6936, 0xf477ea35,
    0xaa64d611, 0x580f5512, 0x4b5fa6e6, 0xb93425e5, 0x6dfe410e, 0x9f95c20d, 0x8cc531f9, 0x7eaeb2fa,
    0x30e349b1, 0xc288cab2, 0xd1d83946, 0x23b3ba45, 0xf779deae, 0x05125dad, 0x1642ae59, 0xe4292d5a,
    0xba3a117e, 0x4851927d, 0x5b016189, 0xa96ae28a, 0x7da08661, 0x8fcb0562, 0x9c9bf696, 0x6ef07595,
    0x417b1dbc, 0xb3109ebf, 0xa0406d4b, 0x522bee48, 0x86e18aa3, 0x748a09a0, 0x67dafa54, 0x95b17957,
    0xcba24573, 0x39c9c670, 0x2a993584, 0xd8f2b687, 0x0c38d26c, 0xfe53516f, 0xed03a29b, 0x1f682198,
    0x5125dad3, 0xa34e59d0, 0xb01eaa24, 0x42752927, 0x96bf4dcc, 0x64d4cecf, 0x77843d3b, 0x85efbe38,
    0xdbfc821c, 0x2997011f, 0x3ac7f2eb, 0xc8ac71e8, 0x1c661503, 0xee0d9600, 0xfd5d65f4, 0x0f36e6f7,
    0x61c69362, 0x93ad1061, 0x80fde395, 0x72966096, 0xa65c047d, 0x5437877e, 0x4767748a, 0xb50cf789,
    0xeb1fcbad, 0x197448ae, 0x0a24bb5a, 0xf84f3859, 0x2c855cb2, 0xdeeedfb1, 0xcdbe2c45, 0x3fd5af46,
    0x7198540d, 0x83f3d70e, 0x90a324fa, 0x62c8a7f9, 0xb602c312, 0x44694011, 0x5739b3e5, 0xa55230e6,
    0xfb410cc2, 0x092a8fc1, 0x1a7a7c35, 0xe811ff36, 0x3cdb9bdd, 0xceb018de, 0xdde0eb2a, 0x2f8b6829,
    0x82f63b78, 0x709db87b, 0x63cd4b8f, 0x91a6c88c, 0x456cac67, 0xb7072f64, 0xa457dc90, 0x563c5f93,
    0x082f63b7, 0xfa44e0b4, 0xe9141340, 0x1b7f9043, 0xcfb5f4a8, 0x3dde77ab, 0x2e8e845f, 0xdce5075c,
    0x92a8fc17, 0x60c37f14, 0x73938ce0, 0x81f80fe3, 0x55326b08, 0xa759e80b, 0xb4091bff, 0x466298fc,
    0x1871a4d8, 0xea1a27db, 0xf94ad42f, 0x0b21572c, 0xdfeb33c7, 0x2d80b0c4, 0x3ed04330, 0xccbbc033,
    0xa24bb5a6, 0x502036a5, 0x4370c551, 0xb11b4652, 0x65d122b9, 0x97baa1ba, 0x84ea524e, 0x7681d14d,
    0x2892ed69, 0xdaf96e6a, 0xc9a99d9e, 0x3bc21e9d, 0xef087a76, 0x1d63f975, 0x0e330a81, 0xfc588982,
    0xb21572c9, 0x407ef1ca, 0x532e023e, 0xa145813d, 0x758fe5d6, 0x87e466d5, 0x94b49521, 0x66df1622,
    0x38cc2a06, 0xcaa7a905, 0xd9f75af1, 0x2b9cd9f2, 0xff56bd19, 0x0d3d3e1a, 0x1e6dcdee, 0xec064eed,
    0xc38d26c4, 0x31e6a5c7, 0x22b65633, 0xd0ddd530, 0x0417b1db, 0xf67c32d8, 0xe52cc12c, 0x1747422f,
    0x49547e0b, 0xbb3ffd08, 0xa86f0efc, 0x5a048dff, 0x8ecee914, 0x7ca56a17, 0x6ff599e3, 0x9d9e1ae0,
    0xd3d3e1ab, 0x21b862a8, 0x32e8915c, 0xc083125f, 0x144976b4, 0xe622f5b7, 0xf5720643, 0x07198540,
    0x590ab964, 0xab613a67, 0xb831c993, 0x4a5a4a90, 0x9e902e7b, 0x6cfbad78, 0x7fab5e8c, 0x8dc0dd8f,
    0xe330a81a, 0x115b2b19, 0x020bd8ed, 0xf0605bee, 0x24aa3f05, 0xd6c1bc06, 0xc5914ff2, 0x37faccf1,
    0x69e9f0d5, 0x9b8273d6, 0x88d28022, 0x7ab90321, 0xae7367ca, 0x5c18e4c9, 0x4f48173d, 0xbd23943e,
    0xf36e6f75, 0x0105ec76, 0x12551f82, 0xe03e9c81, 0x34f4f86a, 0xc69f7b69, 0xd5cf889d, 0x27a40b9e,
    0x79b737ba, 0x8bdcb4b9, 0x988c474d, 0x6ae7c44e, 0xbe2da0a5, 0x4c4623a6, 0x5f16d052, 0xad7d5351,
};

static uint32_t calc_crc32c(uint32_t seed, span<const uint8_t> msg) {
    uint32_t rem = seed;

    for (auto b : msg) {
        rem = crctable[(rem ^ b) & 0xff] ^ (rem >> 8);
    }

    return rem;
}

export namespace btrfs {

constexpr uint64_t superblock_addrs[] = { 0x10000, 0x4000000, 0x4000000000, 0x4000000000000 };

constexpr uint64_t MAGIC = 0x4d5f53665248425f;

constexpr uint64_t FEATURE_INCOMPAT_MIXED_BACKREF = 1 << 0;
constexpr uint64_t FEATURE_INCOMPAT_DEFAULT_SUBVOL = 1 << 1;
constexpr uint64_t FEATURE_INCOMPAT_MIXED_GROUPS = 1 << 2;
constexpr uint64_t FEATURE_INCOMPAT_COMPRESS_LZO = 1 << 3;
constexpr uint64_t FEATURE_INCOMPAT_COMPRESS_ZSTD = 1 << 4;
constexpr uint64_t FEATURE_INCOMPAT_BIG_METADATA = 1 << 5;
constexpr uint64_t FEATURE_INCOMPAT_EXTENDED_IREF = 1 << 6;
constexpr uint64_t FEATURE_INCOMPAT_RAID56 = 1 << 7;
constexpr uint64_t FEATURE_INCOMPAT_SKINNY_METADATA = 1 << 8;
constexpr uint64_t FEATURE_INCOMPAT_NO_HOLES = 1 << 9;
constexpr uint64_t FEATURE_INCOMPAT_METADATA_UUID = 1 << 10;
constexpr uint64_t FEATURE_INCOMPAT_RAID1C34 = 1 << 11;
constexpr uint64_t FEATURE_INCOMPAT_ZONED = 1 << 12;
constexpr uint64_t FEATURE_INCOMPAT_EXTENT_TREE_V2 = 1 << 13;
constexpr uint64_t FEATURE_INCOMPAT_RAID_STRIPE_TREE = 1 << 14;
constexpr uint64_t FEATURE_INCOMPAT_SIMPLE_QUOTA = 1 << 16;

constexpr uint64_t FEATURE_COMPAT_RO_FREE_SPACE_TREE = 1 << 0;
constexpr uint64_t FEATURE_COMPAT_RO_FREE_SPACE_TREE_VALID = 1 << 1;
constexpr uint64_t FEATURE_COMPAT_RO_VERITY = 1 << 2;
constexpr uint64_t FEATURE_COMPAT_RO_BLOCK_GROUP_TREE = 1 << 3;

constexpr uint64_t BLOCK_GROUP_DATA = 1 << 0;
constexpr uint64_t BLOCK_GROUP_SYSTEM = 1 << 1;
constexpr uint64_t BLOCK_GROUP_METADATA = 1 << 2;
constexpr uint64_t BLOCK_GROUP_RAID0 = 1 << 3;
constexpr uint64_t BLOCK_GROUP_RAID1 = 1 << 4;
constexpr uint64_t BLOCK_GROUP_DUP = 1 << 5;
constexpr uint64_t BLOCK_GROUP_RAID10 = 1 << 6;
constexpr uint64_t BLOCK_GROUP_RAID5 = 1 << 7;
constexpr uint64_t BLOCK_GROUP_RAID6 = 1 << 8;
constexpr uint64_t BLOCK_GROUP_RAID1C3 = 1 << 9;
constexpr uint64_t BLOCK_GROUP_RAID1C4 = 1 << 10;

constexpr uint64_t FIRST_CHUNK_TREE_OBJECTID = 0x100;

constexpr uint64_t ROOT_TREE_OBJECTID = 0x1;
constexpr uint64_t EXTENT_TREE_OBJECTID = 0x2;
constexpr uint64_t CHUNK_TREE_OBJECTID = 0x3;
constexpr uint64_t DEV_TREE_OBJECTID = 0x4;
constexpr uint64_t FS_TREE_OBJECTID = 0x5;
constexpr uint64_t ROOT_TREE_DIR_OBJECTID = 0x6;
constexpr uint64_t CSUM_TREE_OBJECTID = 0x7;
constexpr uint64_t UUID_TREE_OBJECTID = 0x9;
constexpr uint64_t FREE_SPACE_TREE_OBJECTID = 0xa;
constexpr uint64_t BLOCK_GROUP_TREE_OBJECTID = 0xb;
constexpr uint64_t EXTENT_CSUM_OBJECTID = 0xfffffffffffffff6;
constexpr uint64_t DATA_RELOC_TREE_OBJECTID = 0xfffffffffffffff7;

using uuid = array<uint8_t, 16>;

struct dev_item {
    uint64_t devid;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint32_t io_align;
    uint32_t io_width;
    uint32_t sector_size;
    uint64_t type;
    uint64_t generation;
    uint64_t start_offset;
    uint32_t dev_group;
    uint8_t seek_speed;
    uint8_t bandwidth;
    btrfs::uuid uuid;
    btrfs::uuid fsid;
} __attribute__((packed));

struct root_backup {
    uint64_t tree_root;
    uint64_t tree_root_gen;
    uint64_t chunk_root;
    uint64_t chunk_root_gen;
    uint64_t extent_root;
    uint64_t extent_root_gen;
    uint64_t fs_root;
    uint64_t fs_root_gen;
    uint64_t dev_root;
    uint64_t dev_root_gen;
    uint64_t csum_root;
    uint64_t csum_root_gen;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t num_devices;
    uint64_t unused_64[4];
    uint8_t tree_root_level;
    uint8_t chunk_root_level;
    uint8_t extent_root_level;
    uint8_t fs_root_level;
    uint8_t dev_root_level;
    uint8_t csum_root_level;
    uint8_t unused_8[10];
} __attribute__((packed));

enum class csum_type : uint16_t {
    CRC32 = 0,
    XXHASH = 1,
    SHA256 = 2,
    BLAKE2 = 3,
};

struct super_block {
    array<uint8_t, 32> csum;
    uuid fsid;
    uint64_t bytenr;
    uint64_t flags;
    uint64_t magic;
    uint64_t generation;
    uint64_t root;
    uint64_t chunk_root;
    uint64_t log_root;
    uint64_t __unused_log_root_transid;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t root_dir_objectid;
    uint64_t num_devices;
    uint32_t sectorsize;
    uint32_t nodesize;
    uint32_t __unused_leafsize;
    uint32_t stripesize;
    uint32_t sys_chunk_array_size;
    uint64_t chunk_root_generation;
    uint64_t compat_flags;
    uint64_t compat_ro_flags;
    uint64_t incompat_flags;
    enum csum_type csum_type;
    uint8_t root_level;
    uint8_t chunk_root_level;
    uint8_t log_root_level;
    btrfs::dev_item dev_item;
    array<char, 0x100> label;
    uint64_t cache_generation;
    uint64_t uuid_tree_generation;
    uuid metadata_uuid;
    uint64_t nr_global_roots;
    uint64_t reserved[27];
    array<uint8_t, 0x800> sys_chunk_array;
    array<root_backup, 4> super_roots;
    uint8_t padding[565];
} __attribute__((packed));

static_assert(sizeof(super_block) == 4096);

enum class key_type : uint8_t {
    INODE_ITEM = 0x01,
    INODE_REF = 0x0c,
    INODE_EXTREF = 0x0d,
    XATTR_ITEM = 0x18,
    VERITY_DESC_ITEM = 0x24,
    VERITY_MERKLE_ITEM = 0x25,
    ORPHAN_INODE = 0x30,
    DIR_LOG_INDEX = 0x48,
    DIR_ITEM = 0x54,
    DIR_INDEX = 0x60,
    EXTENT_DATA = 0x6c,
    EXTENT_CSUM = 0x80,
    ROOT_ITEM = 0x84,
    ROOT_BACKREF = 0x90,
    ROOT_REF = 0x9c,
    EXTENT_ITEM = 0xa8,
    METADATA_ITEM = 0xa9,
    EXTENT_OWNER_REF = 0xac,
    TREE_BLOCK_REF = 0xb0,
    EXTENT_DATA_REF = 0xb2,
    SHARED_BLOCK_REF = 0xb6,
    SHARED_DATA_REF = 0xb8,
    BLOCK_GROUP_ITEM = 0xc0,
    FREE_SPACE_INFO = 0xc6,
    FREE_SPACE_EXTENT = 0xc7,
    FREE_SPACE_BITMAP = 0xc8,
    DEV_EXTENT = 0xcc,
    DEV_ITEM = 0xd8,
    CHUNK_ITEM = 0xe4,
    RAID_STRIPE = 0xe6,
    QGROUP_STATUS = 0xf0,
    QGROUP_INFO = 0xf2,
    QGROUP_LIMIT = 0xf4,
    QGROUP_RELATION = 0xf6,
    TEMPORARY_ITEM = 0xf8,
    PERSISTENT_ITEM = 0xf9,
    DEV_REPLACE = 0xfa,
    UUID_SUBVOL = 0xfb,
    UUID_RECEIVED_SUBVOL = 0xfc,
    STRING_ITEM = 0xfd
};

struct key {
    uint64_t objectid;
    key_type type;
    uint64_t offset;

    bool operator==(const key& k) const = default;

    strong_ordering operator<=>(const key& k) const {
        auto cmp = objectid <=> k.objectid;

        if (cmp != strong_ordering::equal)
            return cmp;

        cmp = type <=> k.type;

        if (cmp != strong_ordering::equal)
            return cmp;

        return offset <=> k.offset;
    }
} __attribute__((packed));

static_assert(sizeof(key) == 17);

struct stripe {
    uint64_t devid;
    uint64_t offset;
    uuid dev_uuid;
} __attribute__((packed));

struct chunk {
    uint64_t length;
    uint64_t owner;
    uint64_t stripe_len;
    uint64_t type;
    uint32_t io_align;
    uint32_t io_width;
    uint32_t sector_size;
    uint16_t num_stripes;
    uint16_t sub_stripes;
    btrfs::stripe stripe[1];
} __attribute__((packed));

struct header {
    array<uint8_t, 32> csum;
    uuid fsid;
    uint64_t bytenr;
    uint64_t flags;
    uuid chunk_tree_uuid;
    uint64_t generation;
    uint64_t owner;
    uint32_t nritems;
    uint8_t level;
} __attribute__((packed));

static_assert(sizeof(header) == 101);

struct item {
    btrfs::key key;
    uint32_t offset;
    uint32_t size;
} __attribute__((packed));

static_assert(sizeof(item) == 25);

struct timespec {
    uint64_t sec;
    uint32_t nsec;
} __attribute__((packed));

struct inode_item {
    uint64_t generation;
    uint64_t transid;
    uint64_t size;
    uint64_t nbytes;
    uint64_t block_group;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    uint64_t rdev;
    uint64_t flags;
    uint64_t sequence;
    uint64_t reserved[4];
    timespec atime;
    timespec ctime;
    timespec mtime;
    timespec otime;
} __attribute__((packed));

static_assert(sizeof(inode_item) == 160);

struct root_item {
    inode_item inode;
    uint64_t generation;
    uint64_t root_dirid;
    uint64_t bytenr;
    uint64_t byte_limit;
    uint64_t bytes_used;
    uint64_t last_snapshot;
    uint64_t flags;
    uint32_t refs;
    key drop_progress;
    uint8_t drop_level;
    uint8_t level;
    uint64_t generation_v2;
    btrfs::uuid uuid;
    btrfs::uuid parent_uuid;
    btrfs::uuid received_uuid;
    uint64_t ctransid;
    uint64_t otransid;
    uint64_t stransid;
    uint64_t rtransid;
    timespec ctime;
    timespec otime;
    timespec stime;
    timespec rtime;
    uint64_t reserved[8];
} __attribute__((packed));

static_assert(sizeof(root_item) == 439);

struct dev_extent {
    uint64_t chunk_tree;
    uint64_t chunk_objectid;
    uint64_t chunk_offset;
    uint64_t length;
    uuid chunk_tree_uuid;
} __attribute__ ((__packed__));

enum class raid_type {
    SINGLE,
    RAID0,
    RAID1,
    DUP,
    RAID10,
    RAID5,
    RAID6,
    RAID1C3,
    RAID1C4,
};

enum raid_type get_chunk_raid_type(const chunk& c) {
    if (c.type & BLOCK_GROUP_RAID0)
        return raid_type::RAID0;
    else if (c.type & BLOCK_GROUP_RAID1)
        return raid_type::RAID1;
    else if (c.type & BLOCK_GROUP_DUP)
        return raid_type::DUP;
    else if (c.type & BLOCK_GROUP_RAID10)
        return raid_type::RAID10;
    else if (c.type & BLOCK_GROUP_RAID5)
        return raid_type::RAID5;
    else if (c.type & BLOCK_GROUP_RAID6)
        return raid_type::RAID6;
    else if (c.type & BLOCK_GROUP_RAID1C3)
        return raid_type::RAID1C3;
    else if (c.type & BLOCK_GROUP_RAID1C4)
        return raid_type::RAID1C4;
    else
        return raid_type::SINGLE;
}

bool check_superblock_csum(const super_block& sb) {
    // FIXME - xxhash, sha256, blake2

    if (sb.csum_type != csum_type::CRC32)
        return false;

    auto crc32 = ~calc_crc32c(0xffffffff,
                              span((uint8_t*)&sb.fsid, sizeof(super_block) - sizeof(sb.csum)));

    return *(uint32_t*)sb.csum.data() == crc32;
}

bool check_tree_csum(const header& h, const super_block& sb) {
    // FIXME - xxhash, sha256, blake2

    if (sb.csum_type != csum_type::CRC32)
        return false;

    auto crc32 = ~calc_crc32c(0xffffffff, span((uint8_t*)&h.fsid, sb.nodesize - sizeof(h.csum)));

    return *(uint32_t*)h.csum.data() == crc32;
}

}

template<>
struct std::formatter<enum btrfs::key_type> {
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin();

        if (it != ctx.end() && *it != '}')
            throw format_error("invalid format");

        return it;
    }

    template<typename format_context>
    auto format(enum btrfs::key_type t, format_context& ctx) const {
        switch (t) {
            case btrfs::key_type::INODE_ITEM:
                return format_to(ctx.out(), "INODE_ITEM");
            case btrfs::key_type::INODE_REF:
                return format_to(ctx.out(), "INODE_REF");
            case btrfs::key_type::INODE_EXTREF:
                return format_to(ctx.out(), "INODE_EXTREF");
            case btrfs::key_type::XATTR_ITEM:
                return format_to(ctx.out(), "XATTR_ITEM");
            case btrfs::key_type::VERITY_DESC_ITEM:
                return format_to(ctx.out(), "VERITY_DESC_ITEM");
            case btrfs::key_type::VERITY_MERKLE_ITEM:
                return format_to(ctx.out(), "VERITY_MERKLE_ITEM");
            case btrfs::key_type::ORPHAN_INODE:
                return format_to(ctx.out(), "ORPHAN_INODE");
            case btrfs::key_type::DIR_LOG_INDEX:
                return format_to(ctx.out(), "DIR_LOG_INDEX");
            case btrfs::key_type::DIR_ITEM:
                return format_to(ctx.out(), "DIR_ITEM");
            case btrfs::key_type::DIR_INDEX:
                return format_to(ctx.out(), "DIR_INDEX");
            case btrfs::key_type::EXTENT_DATA:
                return format_to(ctx.out(), "EXTENT_DATA");
            case btrfs::key_type::EXTENT_CSUM:
                return format_to(ctx.out(), "EXTENT_CSUM");
            case btrfs::key_type::ROOT_ITEM:
                return format_to(ctx.out(), "ROOT_ITEM");
            case btrfs::key_type::ROOT_BACKREF:
                return format_to(ctx.out(), "ROOT_BACKREF");
            case btrfs::key_type::ROOT_REF:
                return format_to(ctx.out(), "ROOT_REF");
            case btrfs::key_type::EXTENT_ITEM:
                return format_to(ctx.out(), "EXTENT_ITEM");
            case btrfs::key_type::METADATA_ITEM:
                return format_to(ctx.out(), "METADATA_ITEM");
            case btrfs::key_type::EXTENT_OWNER_REF:
                return format_to(ctx.out(), "EXTENT_OWNER_REF");
            case btrfs::key_type::TREE_BLOCK_REF:
                return format_to(ctx.out(), "TREE_BLOCK_REF");
            case btrfs::key_type::EXTENT_DATA_REF:
                return format_to(ctx.out(), "EXTENT_DATA_REF");
            case btrfs::key_type::SHARED_BLOCK_REF:
                return format_to(ctx.out(), "SHARED_BLOCK_REF");
            case btrfs::key_type::SHARED_DATA_REF:
                return format_to(ctx.out(), "SHARED_DATA_REF");
            case btrfs::key_type::BLOCK_GROUP_ITEM:
                return format_to(ctx.out(), "BLOCK_GROUP_ITEM");
            case btrfs::key_type::FREE_SPACE_INFO:
                return format_to(ctx.out(), "FREE_SPACE_INFO");
            case btrfs::key_type::FREE_SPACE_EXTENT:
                return format_to(ctx.out(), "FREE_SPACE_EXTENT");
            case btrfs::key_type::FREE_SPACE_BITMAP:
                return format_to(ctx.out(), "FREE_SPACE_BITMAP");
            case btrfs::key_type::DEV_EXTENT:
                return format_to(ctx.out(), "DEV_EXTENT");
            case btrfs::key_type::DEV_ITEM:
                return format_to(ctx.out(), "DEV_ITEM");
            case btrfs::key_type::CHUNK_ITEM:
                return format_to(ctx.out(), "CHUNK_ITEM");
            case btrfs::key_type::RAID_STRIPE:
                return format_to(ctx.out(), "RAID_STRIPE");
            case btrfs::key_type::QGROUP_STATUS:
                return format_to(ctx.out(), "QGROUP_STATUS");
            case btrfs::key_type::QGROUP_INFO:
                return format_to(ctx.out(), "QGROUP_INFO");
            case btrfs::key_type::QGROUP_LIMIT:
                return format_to(ctx.out(), "QGROUP_LIMIT");
            case btrfs::key_type::QGROUP_RELATION:
                return format_to(ctx.out(), "QGROUP_RELATION");
            case btrfs::key_type::TEMPORARY_ITEM:
                return format_to(ctx.out(), "TEMPORARY_ITEM");
            case btrfs::key_type::PERSISTENT_ITEM:
                return format_to(ctx.out(), "PERSISTENT_ITEM");
            case btrfs::key_type::DEV_REPLACE:
                return format_to(ctx.out(), "DEV_REPLACE");
            case btrfs::key_type::UUID_SUBVOL:
                return format_to(ctx.out(), "UUID_SUBVOL");
            case btrfs::key_type::UUID_RECEIVED_SUBVOL:
                return format_to(ctx.out(), "UUID_RECEIVED_SUBVOL");
            case btrfs::key_type::STRING_ITEM:
                return format_to(ctx.out(), "STRING_ITEM");
            default:
                return format_to(ctx.out(), "{:x}", (uint8_t)t);
        }
    }
};

template<>
struct std::formatter<btrfs::key> {
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin();

        if (it != ctx.end() && *it != '}')
            throw format_error("invalid format");

        return it;
    }

    template<typename format_context>
    auto format(const btrfs::key& k, format_context& ctx) const {
        return format_to(ctx.out(), "({:x},{},{:x})", k.objectid, k.type, k.offset);
    }
};
