#if 0
excavator.exe: excavator.obj
 link /nologo /dynamicbase:no /ltcg /machine:x86 /map /merge:.rdata=.text /nxcompat /opt:icf /opt:ref /out:excavator.exe /map:excavator.map /release /debug /pdbaltpath:"%_PDB%" excavator.obj kernel32.lib

excavator.obj: excavator.cpp
 cl /nologo /c /GF /GL /GR- /GS- /Gy /MD /O1ib1 /W4 /WX /Zi /D "NDEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" excavator.cpp

!IF 0
#else

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x600
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <share.h>

typedef unsigned int uint_t;

class option
{
public:
    const wchar_t* optarg;
    int optopt;
    int optind;

private:
    const wchar_t* i_cur;

public:
    option()
        : optarg()
        , optopt()
        , optind(1)
        , i_cur()
    {
    }
    int getopt(int argc, const wchar_t* const* argv, const wchar_t* opts)
    {
        wchar_t ch;
        wchar_t* pch;

#define isoption(c)     (((c) | 2) == '/')
#define isoptarg(s)     (isoption((s)[0]) && (s)[1] != 0)
#define isoptstop(s)    (isoption((s)[1]) && (s)[2] == 0)
#define getcurarg(argv) ((argv)[this->optind])

        if (this->i_cur == NULL) {
            if (this->optind >= argc) {
                return -1;
            } else if (isoptarg(getcurarg(argv)) == 0) {
                return -1;
            } else if (isoptstop(getcurarg(argv))) {
                this->optind++;
                return -1;
            }
            this->i_cur = getcurarg(argv) + 1;
        }

        ch = *this->i_cur;
        this->optopt = ch;

        if (ch == L'=' || (pch = ::wcschr(opts, ch)) == NULL) {
            this->i_cur++;
            if (*this->i_cur == L'\0') {
                this->optind++;
                this->i_cur = NULL;
            }
            return L'?';
        }

        this->i_cur++;

        if (*++pch == L':') {
            this->optind++;
            if (*this->i_cur != L'\0') {
                this->optarg = this->i_cur;
            } else if (this->optind < argc) {
                this->optarg = getcurarg(argv);
                this->optind++;
            } else {
                this->i_cur = NULL;
                return L'?';
            }
            this->i_cur = NULL;
        } else {
            if (*this->i_cur == 0) {
                this->optind++;
                this->i_cur = NULL;
            }
            this->optarg = NULL;
        }

        return ch;
    }
};

class File
{
private:
    int fd;

public:
    File()
        : fd(-1)
    {
    }
    ~File()
    {
        this->close();
    }
    void close()
    {
        if (this->fd != -1) {
            ::_close(this->fd);
            this->fd = -1;
        }
    }
    bool open(const wchar_t* path, bool rw = false)
    {
        this->close();
        return (this->fd = rw ? ::_wopen(path, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE) : ::_wopen(path, O_BINARY | O_RDONLY, S_IREAD)) != -1;
    }
    int64_t size() const
    {
        return ::_filelengthi64(this->fd);
    }
    bool read_at(uint64_t offset, __out_bcount(cb) void* buffer, size_t cb) const
    {
        ::memset(buffer, 0, cb);
        if (::_lseeki64(this->fd, static_cast<int64_t>(offset), SEEK_SET) == -1) {
            return false;
        }
        return static_cast<size_t>(::_read(this->fd, buffer, cb)) == cb;
    }
    bool read(__out_bcount(cb) void* buffer, size_t cb) const
    {
        ::memset(buffer, 0, cb);
        return static_cast<size_t>(::_read(this->fd, buffer, cb)) == cb;
    }
    size_t read_part(__out_bcount(cb) void* buffer, size_t cb, size_t cbBlock) const
    {
        ::memset(buffer, 0, cb);
        const size_t cbRead = static_cast<size_t>(::_read(this->fd, buffer, cb));
        ::_lseeki64(this->fd, cbBlock - cbRead, SEEK_CUR);
        return cbRead;
    }
    bool reset() const
    {
        return ::_lseeki64(this->fd, 0, SEEK_SET) == 0;
    }
    bool copy_to(const File& dst, uint64_t offset, uint64_t cb) const
    {
        if (::_lseeki64(this->fd, static_cast<int64_t>(offset), SEEK_SET) == -1) {
            return false;
        }
        const size_t cbBuff = 512;
        uint8_t buffer[cbBuff];
        while (cb > 0) {
            ::memset(buffer, 0, cbBuff);
            size_t cbToRead = cb < cbBuff ? static_cast<size_t>(cb) : cbBuff;
            size_t cbRead = static_cast<size_t>(::_read(this->fd, buffer, cbToRead));
            if (cbRead == 0 || cbRead > cbToRead) {
                return false;
            }
            size_t cbWritten = static_cast<size_t>(::_write(dst.fd, buffer, cbRead));
            if (cbWritten != cbRead) {
                return false;
            }
            cb -= cbRead;
        }
        return true;
    }
    static bool setfileinfo(bool dir, const wchar_t* path, uint32_t attributes, int64_t created, int64_t accessed, int64_t modified)
    {
        bool result = false;
        HANDLE hf = CreateFileW(path, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, dir ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL);
        if (hf != INVALID_HANDLE_VALUE) {
            FILE_BASIC_INFO info = {};
            info.CreationTime.QuadPart      = created;
            info.LastAccessTime.QuadPart    = accessed;
            info.LastWriteTime.QuadPart     = modified;
            info.ChangeTime.QuadPart        = modified;
            info.FileAttributes             = attributes & 0x27;
            result = ::SetFileInformationByHandle(hf, FileBasicInfo, &info, sizeof(info)) != FALSE;
            ::CloseHandle(hf);
        }
        return result;
    }
};

class exFat
{
private:
#include <pshpack1.h>

    struct entry
    {
        uint8_t type;
        uint8_t remaining[31];
    };

    struct volume_label
    {
        uint8_t type;   // 0x03 or 0x83
        uint8_t cch;
        uint16_t name[11];
        uint8_t reserved[8];
    };

    struct alloc_bitmap
    {
        uint8_t type;   // 0x81
        uint8_t flags;
        uint8_t reserved[18];
        uint32_t first_cluster;
        uint64_t data_length;
    };

    struct upcase_table
    {
        uint8_t type;   // 0x82
        uint8_t reserved1[3];
        uint32_t checksum;
        uint8_t reserved2[12];
        uint32_t first_cluster;
        uint64_t data_length;
    };

    struct file_entry
    {
        uint8_t type;   // 0x05 or 0x85
        uint8_t secondary_count;
        uint16_t checksum;
        uint32_t attributes;
        uint32_t time_created;
        uint32_t time_modified;
        uint32_t time_accessed;
        uint8_t ten_ms_created;
        uint8_t ten_ms_modified;
        uint8_t bias_created;
        uint8_t bias_modified;
        uint8_t bias_accessed;
        uint8_t reserved[7];
    };

    struct stream_extension
    {
        uint8_t type;   // 0x40 or 0xC0
        uint8_t flags;
        uint8_t reserved1;
        uint8_t name_length;
        uint16_t name_hash;
        uint16_t reserved2;
        uint64_t valid_data_length;
        uint32_t reserved3;
        uint32_t first_cluster;
        uint64_t data_length;
    };

    struct file_name_extension
    {
        uint8_t type;   // 0x41 or 0xC1
        uint8_t flags;
        uint16_t name[15];
    };

    struct root_entry
    {
        volume_label vol;
        alloc_bitmap bmp;
        upcase_table upc;
    };

#include <poppack.h>

    struct dos_datetime
    {
        uint16_t year;  // 1980-2107
        uint8_t month;  // 1-12
        uint8_t day;    // 1-31
        uint8_t hour;   // 0-23
        uint8_t minute; // 0-59
        uint8_t second; // 0-59
    };

    uint32_t cluster_size;
    uint32_t reserved_sectors;
    uint32_t upcase_cluster;
    uint32_t fat_cluster;
    uint32_t root_cluster;
    File file;

    template <typename T, size_t count>
    static bool is_all_zero(const T (&array)[count])
    {
        size_t i;
        for (i = 0; i < count && array[i] == 0; i++);
        return i == count;
    }
    static dos_datetime to_dos_datetime(uint32_t datetime)
    {
        dos_datetime tm;
        tm.year = 1980 + ((datetime >> 25) & 0x7f);
        tm.month = (datetime >> 21) & 0xf;
        tm.day = (datetime >> 16) & 0x1f;
        tm.hour = (datetime >> 11) & 0x1f;
        tm.minute = (datetime >> 5) & 0x3f;
        tm.second = (datetime & 0x1f) * 2;
        return tm;
    }
    uint64_t cluster_to_offset(uint32_t cluster) const
    {
        return static_cast<uint64_t>(cluster + this->reserved_sectors - 2) * this->cluster_size;
    }
    uint32_t find_fat_table(uint32_t limit = ~0U) const
    {
        uint32_t fat[2];
        this->file.reset();
        for (uint32_t i = 0; i < limit && this->file.read_part(fat, sizeof(fat), this->cluster_size); i++) {
            if (fat[0] == 0xfffffff8 && fat[1] == 0xffffffff) {
                return i;
            }
        }
        return 0;
    }
    uint32_t find_upcase_table() const
    {
        uint16_t table[32];
        this->file.reset();
        for (uint32_t i = 0; this->file.read_part(table, sizeof(table), this->cluster_size); i++) {
            size_t j, count = sizeof(table) / sizeof(table[0]);
            for (j = 0; j < count && table[j] == j; j++);
            if (j != count) {
                continue;
            }
            return i;
        }
        return 0;
    }
    uint32_t find_root(__out root_entry* found, uint32_t limit = ~0U) const
    {
        root_entry r;
        this->file.reset();
        for (uint32_t i = 0; i < limit && this->file.read_part(&r, sizeof(r), this->cluster_size); i++) {
            if (r.vol.type == 0x03 || r.vol.type == 0x83) {
                if (r.vol.cch > 11) {
                    continue;
                }
            }
            if (is_all_zero(r.vol.reserved) == false) {
                continue;
            }
            if (r.bmp.type != 0x81 || (r.bmp.flags & 0xfe) != 0 || is_all_zero(r.bmp.reserved) == false) {
                continue;
            }
            if (r.upc.type != 0x82 || is_all_zero(r.upc.reserved1) == false || is_all_zero(r.upc.reserved2) == false) {
                continue;
            }
            *found = r;
            return i;
        }
        return 0;
    }
    static bool wildmatch_worker(__in const wchar_t* str, __in const wchar_t* pat)
    {
        for (; *pat != L'\0'; pat++) {
            switch (*pat) {
            case L'?':
                str++;
                break;
            case L'*':
                for (pat++; *str != L'\0'; str++) {
                    if (wildmatch_worker(str, pat)) {
                        return 1;
                    }
                }
                return *pat == L'\0';
            default:
                if (::towlower(*str) != ::towlower(*pat)) {
                    return 0;
                }
                str++;
                break;
            }
        }
        return *str == L'\0';
    }
    // compare with wildcard chars ('*' and '?')
    static bool wildmatch(__in const wchar_t* str, __in const wchar_t* pat)
    {
        // handle special cases
        if (*pat == L'\0' || ::wcscmp(pat, L"*") == 0 || ::wcscmp(pat, L"*.*") == 0) {
            return 1;
        }
        for (const wchar_t* p = str; *p != L'\0'; *p++) {
            const wchar_t ch = *p;
            if (ch == L'/' || ch == L'\\') {
                str = p + 1;
            }
        }
        return wildmatch_worker(str, pat);
    }

public:
    exFat(uint32_t cs, uint32_t rs)
        : cluster_size(cs), reserved_sectors(rs), upcase_cluster(), fat_cluster(), root_cluster()
    {
    }
    bool open(const wchar_t* path)
    {
#define ISPOW2(x)   (((x) & (x) - 1) == 0)
#define KB          * (1ULL << 10)
#define MB          * (1ULL << 20)
#define GB          * (1ULL << 30)

        if (this->file.open(path, false) == false) {
            fprintf(stderr, "cannot open\n");
            return false;
        }
        if (this->cluster_size == 0) {
            // use default cluster size by Microsoft
            // https://support.microsoft.com/en-us/help/140365/default-cluster-size-for-ntfs-fat-and-exfat
            int64_t size = this->file.size();
            if (size < 0) {
                printf("cannot obtain file info\n");
                return false;
            }
            if (size < 256 MB) {
                this->cluster_size = 4 KB;
            } else if (size < 32 GB) {
                this->cluster_size = 32 KB;
            } else {
                this->cluster_size = 128 KB;
            }
        }
        this->upcase_cluster = this->find_upcase_table();
        if (this->upcase_cluster == 0) {
            fprintf(stderr, "no upcase\n");
            return false;
        }
        this->fat_cluster = this->find_fat_table(this->upcase_cluster);
        if (this->fat_cluster == 0) {
            fprintf(stderr, "no fat\n");
            return false;
        }
        root_entry root = {};
        this->root_cluster = this->find_root(&root);
        if (this->root_cluster == 0) {
            fprintf(stderr, "no root\n");
            return false;
        }
        if (this->reserved_sectors == 0) {
            this->reserved_sectors = this->upcase_cluster - root.upc.first_cluster + 2;
        }
        char buff[128];
        sprintf_s(buff, 128, "reserved_sectors: %u, cluster_size: %u, fat_cluster: %u, upcase_cluster: %u, %u, root_cluster: %08I64x (%u)\n",
            reserved_sectors, cluster_size,
            fat_cluster, upcase_cluster, root.upc.first_cluster, static_cast<uint64_t>(root_cluster) * cluster_size, root_cluster);
        fprintf(stderr, "%s", buff);
        //printf("%s", buff);
        return true;
    }
    void enum_files(const wchar_t* outdir, const wchar_t* pattern) const
    {
        printf(
        "   Date      Time    Attr     Entry Offset         Size       Offset      File Offset  Path\n"
        "------------------- ----- ---------------- ------------ ------------ ----------------  ------------------------\n");
        this->enum_files_worker(L"", outdir, pattern, static_cast<uint64_t>(this->root_cluster) * this->cluster_size + sizeof(root_entry), 0);
    }

private:
    static void mkdirs_to_file(wchar_t* path)
    {
        for (wchar_t* p = path; *p != L'\0'; *p++) {
            const wchar_t ch = *p;
            if (ch == L'/' || ch == L'\\') {
                *p = L'\0';
                ::_wmkdir(path);
                *p = ch;
            }
        }
    }
    static int64_t to_filetime(uint32_t dos, uint8_t ten_ms, uint8_t bias)
    {
        if (dos == 0) {
            return 0;
        }
        const dos_datetime tm = to_dos_datetime(dos);
        SYSTEMTIME loc = {};
        FILETIME ft = {};
        loc.wYear   = static_cast<WORD>(tm.year);
        loc.wMonth  = static_cast<WORD>(tm.month);
        loc.wDay    = static_cast<WORD>(tm.day);
        loc.wHour   = static_cast<WORD>(tm.hour);
        loc.wMinute = static_cast<WORD>(tm.minute);
        loc.wSecond = static_cast<WORD>(tm.second + ten_ms % 200) / 100;
        loc.wMilliseconds = static_cast<WORD>(ten_ms * 10 % 1000);
        ::SystemTimeToFileTime(&loc, &ft);
        uint64_t time = ft.dwLowDateTime | static_cast<uint64_t>(ft.dwHighDateTime) << 32;
        if (bias & 0x80) {
            int32_t min = static_cast<int8_t>(bias << 1) >> 1 * 15;
            time += static_cast<int64_t>(min) * 600000000;
        }
        return static_cast<int64_t>(time);
    }
    static void set_file_info(const wchar_t* path, const wchar_t* outdir, const file_entry& fe, bool dir)
    {
        const size_t cchMax = 256 + 16 + MAX_PATH;
        wchar_t outname[cchMax];
        ::wcscpy_s(outname, cchMax, outdir);
        ::wcscat_s(outname, cchMax, path);
        File::setfileinfo(
            dir, outname, fe.attributes,
            to_filetime(fe.time_created, fe.ten_ms_created, fe.bias_created),
            to_filetime(fe.time_accessed, 0, fe.bias_accessed),
            to_filetime(fe.time_modified, fe.ten_ms_modified, fe.bias_modified));
    }
    bool extract_file(const wchar_t* path, const wchar_t* outdir, const stream_extension& se) const
    {
        const size_t cchMax = 256 + 16 + MAX_PATH;
        wchar_t outname[cchMax];
        ::wcscpy_s(outname, outdir);
        ::wcscat_s(outname, path);
        mkdirs_to_file(outname);
        bool result = false;
        File out;
        if (out.open(outname, true) == false) {
            return false;
        }
        const uint64_t file_offset = this->cluster_to_offset(se.first_cluster);
        if ((se.flags & 2) == 2) {
            // normal file
            result = this->file.copy_to(out, file_offset, se.data_length);
        } else {
            // fragmented file; copy only first cluster
            uint64_t remains = se.data_length;
            uint64_t size = remains < this->cluster_size ? remains : this->cluster_size;
            result = this->file.copy_to(out, file_offset, size);
            remains -= size;
            // now look into the file allocation table
            for (uint32_t cluster = se.first_cluster; result && remains > 0; ) {
                if (cluster >= 0xfffffff7 || cluster <= 1) {
                    if (cluster == 0xfffffff7) {
                        fprintf(stderr, "Media descriptor?\n");
                    } else if (cluster == 0xfffffff7) {
                        fprintf(stderr, "Bad cluster?\n");
                    } else if (cluster == 0xffffffff) {
                        fprintf(stderr, "End of file?\n");
                    } else {
                        fprintf(stderr, "0x%08X?\n", cluster);
                    }
                    result = false;
                    break;
                }
                const uint64_t fat_offset = static_cast<uint64_t>(this->fat_cluster) * cluster_size + static_cast<uint64_t>(cluster) * 4;
                uint32_t next_cluster = 0;
                result = this->file.read_at(fat_offset, &next_cluster, sizeof(next_cluster));
                if (result) {
                    //fprintf(stderr, "%08I64X %08X\n", fat_offset, next_cluster);
                    const uint64_t next_offset = this->cluster_to_offset(next_cluster);
                    size = remains < this->cluster_size ? remains : this->cluster_size;
                    result = this->file.copy_to(out, next_offset, size);
                    remains -= size;
                    cluster = next_cluster;
                }
            }
        }
        return result;
    }
    void enum_files_worker(const wchar_t* parent, const wchar_t* outdir, const wchar_t* pattern, uint64_t offset, unsigned int level) const
    {
        file_entry fe;
        for (; this->file.read_at(offset, &fe, sizeof(entry)) && fe.type != 0; ) {
            const uint64_t offent = offset;
            offset += sizeof(entry);
            if ((fe.type & 0x7f) != 0x05) {
                continue;
            }
            if ((fe.type & 0x80) == 0 || fe.secondary_count < 2 || (fe.attributes & ~0xffff) != 0) {
                offset += fe.secondary_count * sizeof(entry);
                continue;
            }
            const size_t cchMax = 256;
            wchar_t path[cchMax] = L"";
            ::wcscat_s(path, cchMax, parent);
            ::wcscat_s(path, cchMax, L"/");
            stream_extension se = {};
            for (uint_t i = 0; i < fe.secondary_count; i++) {
                entry e;
                if (this->file.read_at(offset, &e, sizeof(entry)) == false) {
                    return;
                }
                offset += sizeof(entry);
                if (e.type == 0xC0) {
                    if (se.type == 0) {
                        se = reinterpret_cast<const stream_extension&>(e);
                    }
                } else if (e.type == 0xC1) {
                    const file_name_extension& fn = reinterpret_cast<const file_name_extension&>(e);
                    const size_t cch = ::wcsnlen(reinterpret_cast<const wchar_t*>(fn.name), 15);
                    ::wcsncat_s(path, cchMax, reinterpret_cast<const wchar_t*>(fn.name), cch);
                }
            }
            if (se.first_cluster == 0) {
                continue;
            }
            const uint64_t file_offset = this->cluster_to_offset(se.first_cluster);
            const dos_datetime tm = to_dos_datetime(fe.time_modified);
#define TIMEFMT     "%04u-%02u-%02u %02u:%02u:%02u"
#define TIME(t)     (t).year, (t).month, (t).day, (t).hour, (t).minute, (t).second
#define ATTR(b, t)  ((fe.attributes & (b)) ? (L ## t) : L'.')
            printf(TIMEFMT " %c%c%c%c%c %16I64X %12I64u %12u %16I64X  ",
                TIME(tm),
                ATTR(0x10, 'D'),
                ATTR(0x20, 'A'),
                ATTR(4, 'S'),
                ATTR(2, 'H'),
                ATTR(1, 'R'),
                offent, se.data_length, se.first_cluster, file_offset);
#undef ATTR
            ::wprintf(L"%s", path);
            ::putchar('\n');
            if ((fe.attributes & 0x10) == 0x10) {
                this->enum_files_worker(path, outdir, pattern, file_offset, level + 1);
                set_file_info(path, outdir, fe, true);
            } else if (outdir != NULL && (pattern == NULL || wildmatch(path, pattern))) {
                if (extract_file(path, outdir, se)) {
                    set_file_info(path, outdir, fe, false);
                    fwprintf(stderr, L"Saved: %s\n", path);
                } else {
                    fwprintf(stderr, L"FAIL: %s\n", path);
                }
            }
        }
    }
};

void usage()
{
    fprintf(stderr, "excavator [-c cluster_size] [-r reserved_sectors] [-o out_dir] image.dd\n");
}

int wmain(int argc, wchar_t** argv)
{
    uint32_t cluster_size = 0;
    uint32_t reserved_sectors = 0;
    const wchar_t* out_dir = NULL;
    option opt;

    for (int ch; (ch = opt.getopt(argc, argv, L"c:r:o:")) != -1; ) {
        switch (ch) {
        case L'c':
            cluster_size = wcstoul(opt.optarg, NULL, 10);
            break;
        case L'r':
            reserved_sectors = wcstoul(opt.optarg, NULL, 10);
            break;
        case L'o':
            out_dir = opt.optarg;
            break;
        case L'?':
        default:
            usage();
            return 1;
        }
    }
    if (opt.optind >= argc || ISPOW2(cluster_size) == false) {
        usage();
        return 1;
    }

    const wchar_t* file = argv[opt.optind];
    const wchar_t* pattern = opt.optind + 1 < argc ? argv[opt.optind + 1] : NULL;

    exFat exfat(cluster_size, reserved_sectors);
    if (exfat.open(file)) {
        fwprintf(stderr, L"file: \"%s\"\n", file);
        exfat.enum_files(out_dir, pattern);
        return 1;
    }
    return 2;
}

#endif /*
!ENDIF
#*/
