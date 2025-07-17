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

}
