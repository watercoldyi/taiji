#include "ystring.h"
#include <cstdio>
#include <cassert>
#include <string>

static void test_ctor()
{
    {
        YString s;
        assert(s.length() == 0 && "null str leng != 0");
        assert(s.cstr()[0] == '\0' && "null str != ''");
    }

    {
        printf("test char* ctor\n");
        YString s("abc");
        assert(strcmp(s, "abc") == 0 && "str ctor fail");
    }

    {
        printf("test int ctor\n");
        YString s(10);
        assert(strcmp(s, "10") == 0 && "int ctor fail");
    }
    {
        printf("test double ctor\n");
        YString s(99.9);
        assert(strstr(s, "99.9") == s.cstr() && "double ctor fail");
    }

    {
        printf("test ystring ctor\n");
        YString s2("abc");
        YString s(s2);
        assert(strcmp(s, "abc") == 0 && "ystring ctor fail");
    }

    {
        printf("test char ctor\n");
        YString s('D');
        assert(strcmp(s, "D") == 0 && "char ctor fail");
    }
}

static void test_opt()
{
    {
        printf("test =\n");
        YString s;
        s = "abc";
        assert(strcmp(s, "abc") == 0 && " = const char* fail");
        s = YString("efg");
        assert(strcmp(s, "efg") == 0 && " = const ystring& fail");
    }

    {
        printf("test ==\n");
        YString s("abc");
        assert( s == "abc" && "== const char * fail");
        assert( s == YString("abc") && "== const ystring& fail");
    }

    {
        printf("test +=\n");
        YString s;
        s += "abc";
        assert( s == "abc" && "+= const char * fail");
        s += YString("efg");
        assert( s == "abcefg" && "+= YString fail");
    }

    {
        printf("test +\n");
        YString s = YString("abc") + "efg";
        assert(s == "abcefg" && "+ fail");
    }
}

static void test_substr()
{
    {
        printf("test substr\n");
        YString s("124abc456");
        assert(s.left(3) == "124" && "left fail");
        assert(s.mid(3,3) == "abc" && "mid fail");
        assert(s.right(3) == "456" && "right fail");
    }
}

static void test_compare()
{
    YString s("hello ystring");
    assert(s.compare("hello ystring") && "compare fail");
    assert(s.compareNoCase("HELLO ystring") && "compareNoCase fail");
    assert(s.indexOf("ystr") == 6 && "indexof fail");
    assert(s.indexOfNoCase("YStr") == 6 && "indexof fail");
}

static void test_trime()
{
    printf("test trime\n");
    YString s("  abc1\t ");
    s.trime();
    assert(s == "abc1" && "trime fail");

    printf("test clear");
    s.clear();
    assert(s == "" && "clear fail");
}

static void test_format()
{
    printf("test fmt\n");
    YString s;
    s.format("%d+%d=%s,", 10, 10, "20");
    assert(strcmp(s, "10+10=20,") == 0 && "format fail");
}

static void test_number()
{
    printf("test number\n");
    YString s(100);
    assert(s.toInt() == 100 && "toInt fail");
    s = "99.87";
    assert(s.toDouble() == 99.87 && "toDouble fail");
}

static void test_split()
{
    printf("test split\n");
    YString s = "100,200,name,age";
    auto coll = s.split(",");
    assert(coll.size() == 4 && "split fail expect 4");
    assert(coll[0] == "100" && "coll[0] == 100");
    assert(coll[1] == "200" && "coll[1] == 200");
    assert(coll[2] == "name" && "coll[2] == name");
    assert(coll[3] == "age" && "coll[3] == age");
    s = "100,,200,";
    coll = s.split(",");
    assert(coll.size() == 2 && "split fail expect 2");
    assert(coll[0] == "100" && "coll[0] == 100");
    assert(coll[1] == "200" && "coll[1] == 200");
}

static void test_join()
{
    printf("test join\n");
    YString s;
    s.join({ "123","name","age" }, "&");
    assert(strcmp(s, "123&name&age") == 0 && "join fail");
}

static void test_bigstr()
{
    printf("test bigstr");
    const char *s2 = "abcdefg1234567890";
    int ns2 = strlen(s2);
    YString s;
    std::string ss;
    for (int i = 0; i < 1024; i++) {
        s += s2;
        ss += s2;
    }
    assert(s.length() == ns2 * 1024 && "bigstr size invalid");
    assert(ss == (const char *)s && "bigstr invalid");
}

int main()
{
    test_ctor();
    test_opt();
    test_substr();
    test_compare();
    test_trime();
    test_format();
    test_number();
    test_split();
    test_join();
    test_bigstr();
    return 0;
}