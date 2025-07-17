module;

#include <stdint.h>
#include <array>

export module cxxbtrfs;

using namespace std;

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
    uint16_t csum_type;
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

}
