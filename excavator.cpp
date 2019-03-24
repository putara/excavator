#if 0
excavator.exe: excavator.obj
 link /nologo /dynamicbase:no /ltcg /machine:x86 /map /merge:.rdata=.text /nxcompat /opt:icf /opt:ref /out:excavator.exe /map:excavator.map /release /debug /pdbaltpath:"%_PDB%" excavator.obj kernel32.lib

excavator.obj: excavator.cpp option.hpp
 cl /nologo /c /GF /GL /GR- /GS- /Gy /MD /O1ib1 /W4 /WX /Zi /D "NDEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" excavator.cpp

!IF 0
#else

#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <conio.h>
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
#include "option.hpp"

typedef unsigned int uint_t;

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
    bool open(const wchar_t* path, bool write = false)
    {
        this->close();
        return (this->fd = write ? ::_wopen(path, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE) : ::_wopen(path, O_BINARY | O_RDONLY, S_IREAD)) != -1;
    }
    bool reopen(const File& src, bool write = false)
    {
        this->close();
        if (src.fd == -1) {
            return false;
        }
        HANDLE srchf = reinterpret_cast<HANDLE>(::_get_osfhandle(src.fd));
        if (srchf == INVALID_HANDLE_VALUE) {
            return false;
        }
        HANDLE dsthf = write ? ::ReOpenFile(srchf, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0) : ::ReOpenFile(srchf, GENERIC_READ, FILE_SHARE_READ, 0);
        if (dsthf == INVALID_HANDLE_VALUE) {
            return false;
        }
        if ((this->fd = ::_open_osfhandle(reinterpret_cast<intptr_t>(dsthf), O_BINARY)) == -1) {
            ::CloseHandle(dsthf);
            return false;
        }
        return true;
    }
    int64_t size() const
    {
        return ::_filelengthi64(this->fd);
    }
    bool seek_at(uint64_t offset) const
    {
        return ::_lseeki64(this->fd, static_cast<int64_t>(offset), SEEK_SET) == static_cast<int64_t>(offset);
    }
    bool read_all_at(uint64_t offset, __out_bcount(cb) void* buffer, unsigned int cb) const
    {
        return this->read_n_at(offset, buffer, cb) == cb;
    }
    unsigned int read_n_at(uint64_t offset, __out_bcount(cb) void* buffer, unsigned int cb) const
    {
        if (this->seek_at(offset) == false) {
            ::memset(buffer, 0, cb);
            return 0;
        }
        return this->read_n(buffer, cb);
    }
    bool read_all(__out_bcount(cb) void* buffer, unsigned int cb) const
    {
        return this->read_n(buffer, cb) == cb;
    }
    unsigned int read_n(__out_bcount(cb) void* buffer, unsigned int cb) const
    {
        ::memset(buffer, 0, cb);
        const int ret = ::_read(this->fd, buffer, cb);
        return ret < 0 ? 0 : static_cast<unsigned int>(ret);
    }
    unsigned int read_part(__out_bcount(cb) void* buffer, unsigned int cb, unsigned int cbBlock) const
    {
        const unsigned int cbRead = this->read_n(buffer, cb);
        return ::_lseeki64(this->fd, cbBlock - cbRead, SEEK_CUR) != -1 ? cbRead : 0;
    }
    unsigned int write_n(__in_bcount(cb) const void* buffer, unsigned int cb) const
    {
        const int ret = ::_write(this->fd, buffer, cb);
        return ret < 0 ? 0 : static_cast<unsigned int>(ret);
    }
    bool reset() const
    {
        return this->seek_at(0);
    }
    static bool setfileinfo(bool dir, const wchar_t* path, uint32_t attributes, int64_t created, int64_t accessed, int64_t modified)
    {
        bool result = false;
        HANDLE hf = ::CreateFileW(path, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, dir ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL);
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

    class exFatFile
    {
    private:
        const exFat& owner;
        File file;
        uint32_t cluster;
        uint8_t* buff;
        unsigned int pos;
        unsigned int cbFilled;
        unsigned int cbBuff;
        uint64_t cbValid;
        uint64_t cbFile;
        bool chain;

        exFatFile(const exFatFile&);
        exFatFile& operator =(const exFatFile&);

        uint32_t get_next_cluster() const
        {
            if (this->chain == false) {
                return this->cluster + 1;
            }
            const uint64_t fat_offset = this->owner.get_fat_offset(this->cluster);
            uint32_t next_cluster = 0;
            if (this->file.read_all_at(fat_offset, &next_cluster, sizeof(next_cluster)) == false) {
                return 0;
            }
            if (next_cluster > 0xfffffff6 || next_cluster <= 1) {
                if (next_cluster == 0xfffffff7) {
                    fprintf(stderr, "%I64X [%08X] Bad cluster\n", fat_offset, next_cluster);
                } else if (next_cluster == 0xfffffff8) {
                    fprintf(stderr, "%I64X [%08X] Media descriptor\n", fat_offset, next_cluster);
                } else if (next_cluster == 0xffffffff) {
                    //fprintf(stderr, "%I64X [%08X] End of file\n", fat_offset, next_cluster);
                } else {
                    fprintf(stderr, "%I64X [%08X] \?\?\? FAT corrupted\n", fat_offset, next_cluster);
                }
                return 0;
            }
            return next_cluster;
        }
        bool refill()
        {
            this->cbFilled = this->pos = 0;
            if (this->cluster == 0) {
                return false;
            }
            if (this->cbFile == 0) {
                return false;
            }
            const unsigned int cbToRead = this->cbFile < this->cbBuff ? static_cast<unsigned int>(this->cbFile) : this->cbBuff;
            if (this->cbValid == 0) {
                this->cbFile -= cbToRead;
                this->cbFilled = cbToRead;
                ::memset(this->buff, 0, cbToRead);
                return true;
            }
            const uint64_t offset = this->owner.cluster_to_offset(this->cluster);
            this->cluster = this->get_next_cluster();
            const unsigned int cbReadable = this->cbValid < cbToRead ? static_cast<unsigned int>(this->cbValid) : cbToRead;
            const unsigned int cbRead = this->file.read_n_at(offset, this->buff, cbReadable);
            if (cbRead < cbToRead) {
                ::memset(this->buff + cbRead, 0, cbToRead - cbRead);
            }
            this->cbFile -= cbToRead;
            this->cbValid -= cbReadable;
            this->cbFilled = cbToRead;
            return true;
        }
        bool open_worker(uint32_t cluster, bool chain)
        {
            this->file.close();
            this->cluster = cluster;
            this->cbFilled = this->pos = 0;
            this->chain = chain;
            if (this->buff == NULL && (this->buff = static_cast<uint8_t*>(::malloc(this->cbBuff))) == NULL) {
                return false;
            }
            return this->file.reopen(this->owner.file, false);
        }

    public:
        exFatFile(const exFat& self)
            : owner(self)
            , cluster()
            , buff()
            , pos(0)
            , cbFilled(0)
            , cbBuff(self.cluster_size)
            , cbValid()
            , cbFile()
            , chain()
        {
        }
        ~exFatFile()
        {
            ::free(this->buff);
        }
        bool open(uint32_t cluster)
        {
            this->cbFile = this->cbValid = ~0ULL;
            return this->open_worker(cluster, false);
        }
        bool open(const stream_extension& se)
        {
            this->cbValid = se.valid_data_length;
            this->cbFile = se.data_length;
            if (this->cbValid > this->cbFile) {
                this->cbValid = this->cbFile;
            }
            return this->open_worker(se.first_cluster, (se.flags & 2) == 0);
        }
        size_t read(void* buffer, size_t cb)
        {
            uint8_t* p = static_cast<uint8_t*>(buffer);
            if (p != NULL) {
                ::memset(p, 0, cb);
            }
            size_t cbRead = 0;
            while (cb > 0) {
                if (this->pos >= this->cbFilled) {
                    if (this->refill() == false) {
                        break;
                    }
                }
                size_t cbToRead = this->cbFilled - this->pos;
                if (cbToRead > cb) {
                    cbToRead = cb;
                }
                if (p != NULL) {
                    ::memcpy(p, this->buff + this->pos, cbToRead);
                    p += cbToRead;
                }
                this->pos += cbToRead;
                cb -= cbToRead;
                cbRead += cbToRead;
            }
            return cbRead;
        }
        bool copy_to(const File& dst, uint64_t cb)
        {
            const unsigned int SIZE = 4096;
            uint8_t buffer[SIZE];
            while (cb > 0) {
                const size_t cbToRead = cb < SIZE ? static_cast<size_t>(cb) : SIZE;
                const size_t cbRead = this->read(buffer, cbToRead);
                if (cbRead == 0 || cbRead > cbToRead) {
                    return false;
                }
                const size_t cbWritten = dst.write_n(buffer, cbRead);
                if (cbWritten != cbRead) {
                    return false;
                }
                cb -= cbRead;
            }
            return true;
        }
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
    uint64_t get_fat_offset(uint32_t cluster) const
    {
        return static_cast<uint64_t>(this->fat_cluster) * this->cluster_size + static_cast<uint64_t>(cluster) * 4;
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
            fprintf(stderr, "No upcase table found\n");
            return false;
        }
        this->fat_cluster = this->find_fat_table(this->upcase_cluster);
        if (this->fat_cluster == 0) {
            fprintf(stderr, "No file allocation table found\n");
            return false;
        }
        root_entry root = {};
        const uint32_t root_cluster = this->find_root(&root);
        if (root_cluster == 0) {
            fprintf(stderr, "No root directory found\n");
            return false;
        }
        if (this->reserved_sectors == 0) {
            this->reserved_sectors = this->upcase_cluster - root.upc.first_cluster + 2;
        }
        if (root_cluster <= this->reserved_sectors) {
            fprintf(stderr, "No root directory found\n");
            return false;
        }
        this->root_cluster = root_cluster - this->reserved_sectors + 2;
        char buff[128];
        sprintf_s(buff, 128, "reserved_sectors: %u, cluster_size: %u, fat_cluster: %u, upcase_cluster: %u, %u, root_cluster: %08I64x (%u)\n",
            reserved_sectors, cluster_size,
            fat_cluster, upcase_cluster, root.upc.first_cluster, static_cast<uint64_t>(root_cluster) * cluster_size, this->root_cluster);
        _cprintf("%s", buff);
        return true;
    }
    void enum_files(const wchar_t* outdir, const wchar_t* pattern) const
    {
        exFatFile dir(*this);
        if (dir.open(this->root_cluster) == false || dir.read(NULL, sizeof(root_entry)) != sizeof(root_entry)) {
            return;
        }
        printf(
        "   Date      Time    Attr         Size      Cluster      File Offset  Path\n"
        "------------------- ----- ------------ ------------ ----------------  ------------------------\n");
        this->enum_files_worker(dir, L"", outdir, pattern, 0);
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
        loc.wSecond = static_cast<WORD>(tm.second) + static_cast<WORD>(ten_ms % 200 / 100);
        loc.wMilliseconds = static_cast<WORD>(ten_ms * 10 % 1000);
        ::SystemTimeToFileTime(&loc, &ft);
        uint64_t time = ft.dwLowDateTime | static_cast<uint64_t>(ft.dwHighDateTime) << 32;
        if ((bias & 0x80) == 0x80) {
            const int32_t minutes = -(static_cast<int8_t>((bias & 0x7f) << 1) >> 1) * 15;
            time += static_cast<int64_t>(minutes) * 600000000;
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
        File out;
        if (out.open(outname, true) == false) {
            return false;
        }
        exFatFile in(*this);
        if (in.open(se) == false) {
            return false;
        }
        return in.copy_to(out, se.data_length);
    }
    void enum_files_worker(exFatFile& dir, const wchar_t* parent, const wchar_t* outdir, const wchar_t* pattern, unsigned int level) const
    {
        file_entry fe;
        for (; dir.read(&fe, sizeof(fe)) == sizeof(fe) && fe.type != 0; ) {
            //const uint64_t offent = 0;  // TODO: fill this variable
            if ((fe.type & 0x7f) != 0x05) {
                continue;
            }
            if ((fe.type & 0x80) == 0 || fe.secondary_count < 2 || (fe.attributes & ~0xffff) != 0) {
                if (dir.read(NULL, fe.secondary_count * sizeof(entry)) == false) {
                    break;
                }
                continue;
            }
            const size_t cchMax = 256;
            wchar_t path[cchMax] = L"";
            ::wcscat_s(path, cchMax, parent);
            ::wcscat_s(path, cchMax, L"/");
            stream_extension se = {};
            for (uint_t i = 0; i < fe.secondary_count; i++) {
                entry e;
                if (dir.read(&e, sizeof(e)) == false) {
                    return;
                }
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
            const uint64_t file_offset = this->cluster_to_offset(se.first_cluster);
            const dos_datetime tm = to_dos_datetime(fe.time_modified);
#define TIMEFMT     "%04u-%02u-%02u %02u:%02u:%02u"
#define TIME(t)     (t).year, (t).month, (t).day, (t).hour, (t).minute, (t).second
#define ATTR(b, t)  ((fe.attributes & (b)) ? (L ## t) : L'.')
            printf(TIMEFMT " %c%c%c%c%c %12I64u %12u %16I64X  ",
                TIME(tm),
                ATTR(0x10, 'D'),
                ATTR(0x20, 'A'),
                ATTR(4, 'S'),
                ATTR(2, 'H'),
                ATTR(1, 'R'),
                //offent,
                se.data_length, se.first_cluster, file_offset);
#undef ATTR
#undef TIME
#undef TIMEFMT
            ::wprintf(L"%s", path);
            ::putchar('\n');
            if ((fe.attributes & 0x10) == 0x10) {
                exFatFile child(*this);
                if (child.open(se)) {
                    this->enum_files_worker(child, path, outdir, pattern, level + 1);
                } else {
                    fwprintf(stderr, L"Failed to open: %s\n", path);
                }
                if (outdir != NULL) {
                    set_file_info(path, outdir, fe, true);
                }
            } else if (outdir != NULL && (pattern == NULL || wildmatch(path, pattern))) {
                if (extract_file(path, outdir, se)) {
                    set_file_info(path, outdir, fe, false);
                    _cwprintf(L"Saved: %s\n", path);
                } else {
                    _cwprintf(L"FAIL: %s\n", path);
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
