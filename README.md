# Introduction
Recover files from an accidentally formatted exFAT drive such as SDXC/microSDXC card.  
Unlike signature-based recovery software, the excavator uses a heuristic approach to infer the missing master boot record.

## Features
The excavator provides some unique features which ordinary recovery software (even commercial software) cannot offer.

- Way faster to scan files (as fast as `dir /s` or `ls -lR` command)
- Ability to recover fragmented files provided FAT is still intact
- Very small program size

**This is not undelete software.** Previously deleted files are excluded and cannot be recovered.

## Prerequisites
- Windows Vista/7/8/8.1/10
- Microsoft Visual C++
- A raw disk image of an accidentally formatted exFAT partition

## Usage
`excavator [-c cluster_size] [-r reserved_sectors] [-o out_dir] [-l out_list] [-i out_info] image.dd [pattern]`

1. Create a disk image using the `dd` command or [this tool](http://hddguru.com/software/HDD-Raw-Copy-Tool/)
2. Run `excacator` from CLI
3. If no files are found, try different cluster size
4. RTFC for more info

## Build
1. Open Developer console
1. Run `nmake`

## Todos
- [X] Support fragmented directory entries
- [ ] Support non-Windows operating systems
- [ ] Automatically find out cluster size
- [ ] Better pattern matching (regex)

## Acknowledgements
- [Reverse Engineering the Microsoft exFAT File System](https://www.sans.org/reading-room/whitepapers/forensics/paper/33274) by Robert Shullich
- [A minimal POSIX getopt()](https://github.com/skeeto/getopt)

## See also
- [The Sleuth Kit](https://github.com/sleuthkit/sleuthkit)

## License
The excavator code is released under the [GPL v3](https://www.gnu.org/licenses/gpl-3.0.en.html).
The option class and its tests are public domain.
