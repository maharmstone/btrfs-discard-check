btrfs-discard-check
===================

`btrfs-discard-check` is a tool to make sure that the discard function of btrfs
is working correctly - that the underlying hardware is made aware of any free
space. This works by using Qemu's qcow2 format, which tracks discards.

Compilation
-----------

You will need a recent version of CMake, Ninja, and at least GCC 15 (as we're
using C++ modules). [nlohmann-json](https://github.com/nlohmann/json) is also
required.

```
mkdir build
cd build
cmake -GNinja ..
```

There is also a runtime dependency on qemu-img, which comes with qemu.

Usage
-----

Create a qcow image:

`qemu-img create -f qcow2 -o cluster_size=4096 test.img 1G`

Make sure you specify `cluster_size=4096`, as it defaults to a 64KB discard
granularity otherwise.

Make it appear as a block device:

`qemu-nbd -c /dev/nbd0 --discard=unmap test.img`

Or in a VM:

```
qemu-system_x86_64 ... \
-device virtio-scsi-pci,id=scsi0 \
-drive file=test.img,if=none,discard=unmap,id=test \
-device scsi-hd,drive=test,bus=scsi0.0 \
...

```

In both cases, `discard=unmap` is important.

Do `mkfs.btrfs`, mount the filesystem, and manipulate it as you want.

Unmount the filesystem, and use `qemu-nbd -d /dev/nbd0` to remove the block
device (or stop the VM).

Run `btrfs-discard-check` against the image:

```
$ ./btrfs-discard-check test.img
free space entry 100000, 400000 not part of any chunk
free space entry 500000, 800000 not part of any chunk
qcow range 2500000, 4000 allocated (address 1d00000) but is free space
qcow range 2520000, c000 allocated (address 1d20000) but is free space
qcow range 5830000, 4000 allocated (address 1d00000) but is free space
qcow range 5850000, c000 allocated (address 1d20000) but is free space
```

To do
-----

* Multiple devices
* RAID0, RAID10, RAID5, RAID6
* Other hash algorithms (xxhash, SHA-256, Blake2)
* Remove reliance on `qemu-img`
* Understand the log tree
