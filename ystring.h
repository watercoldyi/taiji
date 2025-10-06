#pragma once

#include <cstring>
#include <cstdint>
#include <vector>

#define YSTR_SBUF_SIZE  128

class YString
{
public:
    YString();
    YString(const char *str);
    YString(const char *str,int len);
    YString(const YString &str);
    YString(char c);
    YString(double v);
    YString(int64_t v);
    YString(int v) : YString((int64_t)v) {};

    ~YString();

    YString & operator=(const char *s);
    YString & operator=(const YString &s);

    bool operator ==(const char* s);
    
    YString & operator+=(const char *s);
    YString & operator+=(const YString &s);

    YString operator+(const char* s);
    YString operator+(const YString &s);

    operator const char *() const {return _str;};

    int length() const {return _len;};
    bool empty() const {return _len == 0;};
    const char *cstr() const {return _str;};

    YString& append(const char* s, int len);

    YString left(int n) const;
    YString mid(int from,int n) const;
    YString right(int n) const;

    void toUpper();
    void toLower();

    int64_t toInt() const;
    double toDouble() const;

    YString & format(const char *fmt,...);

    int indexOf(const char *str);
    int indexOfNoCase(const char *str);
    bool compare(const char *s);
    bool compareNoCase(const char *s);

    void clear();
    void trime();

    std::vector<YString> split(const char *token);
    YString & join(const std::vector<YString> &ss, char *s);

private:
    char _sbuf[YSTR_SBUF_SIZE];
    char *_str;
    int _len;
    int _cap;
};