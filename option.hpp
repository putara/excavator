// the option class is based on the public domain code found at https://github.com/skeeto/getopt

#pragma once
#include <string.h>

class option
{
public:
    const wchar_t* optarg;
    int optopt;
    int optind;

private:
    int optpos;

    static bool isoption(wchar_t ch)
    {
        //return (ch | 2) == L'/';
        return ch == L'-';
    }
    static bool isoptarg(const wchar_t* s)
    {
        return isoption(s[0]) && s[1] != L'\0';
    }
    static bool isoptstop(const wchar_t* s)
    {
        return isoption(s[0]) && isoption(s[1]) && s[2] == 0;
    }
    const wchar_t* getcurarg(int argc, const wchar_t* const* argv, int i) const
    {
        // the buggy WinXP's CommandLineToArgvW() does not set argv[argc] to NULL
        return i < argc ? argv[i] : NULL;
    }

public:
    option()
        : optarg()
        , optopt()
        , optind(1)
        , optpos(1)
    {
    }
    int getopt(int argc, const wchar_t* const* argv, const wchar_t* optstring)
    {
        const wchar_t* arg = this->getcurarg(argc, argv, this->optind);
        const wchar_t* opt;
        wchar_t ch;
        if (arg != NULL && isoptstop(arg)) {
            this->optind++;
            return -1;
        } else if (arg == NULL || isoptarg(arg) == false) {
            return -1;
        }
        this->optopt = ch = arg[this->optpos];
        if ((opt = ::wcschr(optstring, ch)) == NULL) {
            return L'?';
        }
        if (opt[1] == L':') {
            opt = arg + this->optpos + 1;
            if (*opt != L'\0') {
                this->optarg = opt;
                this->optind++;
                this->optpos = 1;
                return ch;
            } else if ((opt = this->getcurarg(argc, argv, this->optind + 1)) != NULL) {
                this->optarg = opt;
                this->optind += 2;
                this->optpos = 1;
                return ch;
            } else {
                return *optstring == L':' ? L':' : L'?';
            }
        } else {
            if (arg[++this->optpos] == L'\0') {
                this->optind++;
                this->optpos = 1;
            }
            return ch;
        }
    }
};


#ifdef TEST_OPTION
#include <stdio.h>

class test_runner
{
private:
    const wchar_t* name;
    static int count_pass;
    static int count_fail;

public:
    test_runner(const wchar_t* const* argv)
    {
        this->name = *argv;
    }
    void assert_equals(const wchar_t* s, int x, int y)
    {
        if (x == y) {
            wprintf(L"\033[32;1mPASS\033[0m %s %s\n", this->name, s);
            count_pass++;
        } else {
            if (x < L' ') {
                wprintf(L"\033[31;1mFAIL\033[0m %s %s ; %d != %d\n", this->name, s, x, y);
            } else {
                wprintf(L"\033[31;1mFAIL\033[0m %s %s ; '%c' != '%c'\n", this->name, s, x, y);
            }
            count_fail++;
        }
    }
    void assert_equals(const wchar_t* s, const wchar_t* x, const wchar_t* y)
    {
        if ((x != NULL) == (y != NULL) && ::wcscmp(x, y) == 0) {
            wprintf(L"\033[32;1mPASS\033[0m %s %s\n", this->name, s);
            count_pass++;
        } else {
            wprintf(L"\033[31;1mFAIL\033[0m %s %s ; \"%s\" != \"%s\"\n", this->name, s, x, y);
            count_fail++;
        }
    }
    static int finish()
    {
        wprintf(L"%d fail, %d pass\n", count_fail, count_pass);
        return count_fail != 0;
    }
};

int test_runner::count_pass;
int test_runner::count_fail;

template <int c> inline int ARGC(wchar_t* (&)[c])
{
    return c - 2;
}

#define SENTINEL L"#######################"
#define END SENTINEL, NULL

static void test0(void)
{
    wchar_t* argv[] = {
        L"test0",
        L"-a", L"alpha",
        L"-bbeta",
        L"-c", L"gamma",
        L"--",
        L"file",
        END
    };
    int argc = ARGC(argv);
    test_runner test(argv);
    option opt;
    test.assert_equals(L"a", L'a', opt.getopt(argc, argv, L"a:b:c:"));
    test.assert_equals(L"a optarg", argv[2], opt.optarg);
    test.assert_equals(L"b", L'b', opt.getopt(argc, argv, L"a:b:c:"));
    test.assert_equals(L"b optarg", argv[3] + 2, opt.optarg);
    test.assert_equals(L"c", L'c', opt.getopt(argc, argv, L"a:b:c:"));
    test.assert_equals(L"c optarg", argv[5], opt.optarg);
    test.assert_equals(L"finish", -1, opt.getopt(argc, argv, L"a:b:c:"));
    test.assert_equals(L"optind", argv[argc - 1], argv[opt.optind]);
}

static void test1(void)
{
    wchar_t *argv[] = {
        L"test1",
        L"-abcfoo",
        END
    };
    int argc = ARGC(argv);
    test_runner test(argv);
    option opt;
    test.assert_equals(L"a", L'a', opt.getopt(argc, argv, L"abc:"));
    test.assert_equals(L"b", L'b', opt.getopt(argc, argv, L"abc:"));
    test.assert_equals(L"c", L'c', opt.getopt(argc, argv, L"abc:"));
    test.assert_equals(L"a", -1, opt.getopt(argc, argv, L"abc:"));
    test.assert_equals(L"c optarg", argv[1] + 4, opt.optarg);
    test.assert_equals(L"optind", SENTINEL, argv[opt.optind]);
    test.assert_equals(L"stop 1", -1, opt.getopt(argc, argv, L"abc:"));
    test.assert_equals(L"c optarg", 2, opt.optind);
    test.assert_equals(L"stop 2", -1, opt.getopt(argc, argv, L"abc:"));
    test.assert_equals(L"c optarg", 2, opt.optind);
}

static void test2(void)
{
    wchar_t *argv[] = {
        L"test2",
        L"-a",
        END
    };
    int argc = ARGC(argv);
    test_runner test(argv);
    option opt1;
    test.assert_equals(L"missing argument", L':', opt1.getopt(argc, argv, L":a:bc"));
    option opt2;
    test.assert_equals(L"missing argument", L'?', opt2.getopt(argc, argv, L"a:bc"));
    test.assert_equals(L"optopt", 'a', opt2.optopt);
}

static void test3(void)
{
    wchar_t *argv[] = {
        L"test3",
        L"-x",
        END
    };
    int argc = ARGC(argv);
    test_runner test(argv);
    option opt1;
    test.assert_equals(L"unknown option 1", L'?', opt1.getopt(argc, argv, L"a:bc"));
    option opt2;
    test.assert_equals(L"unknown option 2", L'?', opt2.getopt(argc, argv, L":a:bc"));
    test.assert_equals(L"optopt", 'x', opt2.optopt);
}

static void test4(void)
{
    wchar_t *argv[] = {
        L"test4",
        L"-ayfoo",
        END
    };
    int argc = ARGC(argv);
    test_runner test(argv);
    option opt;
    test.assert_equals(L"a", L'a', opt.getopt(argc, argv, L"abc"));
    test.assert_equals(L"unknown option", L'?', opt.getopt(argc, argv, L":abc"));
    test.assert_equals(L"optopt", L'y', opt.optopt);
    test.assert_equals(L"unknown option stick", L'?', opt.getopt(argc, argv, L":abc"));
    test.assert_equals(L"optopt stick", L'y', opt.optopt);
}

static void test5(void)
{
    wchar_t *argv[] = {
        L"test5",
        L"-a",
        L"--",
        L"-b",
        L"-ca"
        END
    };
    int argc = ARGC(argv);
    test_runner test(argv);
    option opt;
    test.assert_equals(L"a", L'a', opt.getopt(argc, argv, L"abc"));
    test.assert_equals(L"finish", -1, opt.getopt(argc, argv, L"abc"));
    test.assert_equals(L"optind", L"-b", argv[opt.optind]);
}

int main(void)
{
    test0();
    test1();
    test2();
    test3();
    test4();
    test5();
    return test_runner::finish();
}

#endif
