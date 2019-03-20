# Introduction
Recover files from an accidentally formatted exFAT partition such as SDXC/microSDXC card.  
Unlike signature-based recovery software, the `excavator` uses a heuristic approach to locate mandatory system files such as file allocation table, upper case table and volume label to traverse directory tree. Therefore, it is not only way faster to scan files but it can also recover fragmented files.

## Prerequisites
- Windows
- Microsoft Visual C++
- A raw disk image of an accidentally formatted exFAT partition

## Usage
`excavator [-c cluster_size] [-r reserved_sectors] [-o out_dir] image.dd`

## Build
1. Open Developer console
1. Run `nmake /f excavator.cpp`

## Todos
- [ ] Support fragmented directory entries
- [ ] Support non-Windows operating systems

## Acknowledgements
- [Reverse Engineering the Microsoft exFAT File System](https://www.sans.org/reading-room/whitepapers/forensics/paper/33274) by Robert Shullich

## See also
- [The Sleuth Kit](https://github.com/sleuthkit/sleuthkit)

## License

TBA
