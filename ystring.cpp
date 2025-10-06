#include "ystring.h"
#include <cstdarg>

YString::YString()
{
    _sbuf[0] = '\0';
    _len = 0;
    _cap = YSTR_SBUF_SIZE - 1;
    _str = _sbuf;
}

YString::YString(const char *str) : YString(str,strlen(str))
{
}

YString::YString(const char *str, int len)
{
    if (len > YSTR_SBUF_SIZE - 1) {
        _str = new char[len + YSTR_SBUF_SIZE];
        _cap = len + YSTR_SBUF_SIZE - 1;
    }
    else {
        _str = _sbuf;
        _cap = YSTR_SBUF_SIZE - 1;
    }
    memcpy(_str,str,len);
    _str[len] = '\0';
    _len = len;
}

YString::YString(const YString &str) : YString((const char *)str,str._len)
{
}

YString::YString(char c) : YString()
{
    _str[0] = c;
    _str[1] = '\0';
    _len = 1;
}

YString::YString(double v) : YString()
{
    format("%f", v);
}

YString::YString(int64_t v) : YString()
{
    format("%llu", v);
}

YString::~YString()
{
    if (_str != _sbuf) {
        delete[] _str;
    }
}

YString &YString::operator=(const char *s)
{
    int len = strlen(s);
    if (len >= _cap) {
        _cap = len + YSTR_SBUF_SIZE - 1;
        if (_str != _sbuf) {
            delete[] _str;
        }
        _str = new char[_cap];
    }
    memcpy(_str,s,len);
    _str[len] = '\0';
    _len = len;
    return *this;
}

YString &YString::operator=(const YString &s)
{
    if (s._len >= _cap) {
        _cap = s._len + YSTR_SBUF_SIZE - 1;
        if (_str != _sbuf) {
            delete[] _str;
        }
        _str = new char[_cap];
    }
    memcpy(_str,s,s._len);
    _str[s._len] = '\0';
    _len = s._len;
    return *this;
}

bool YString::operator==(const char* s)
{
    return compare(s);
}

YString &YString::operator+=(const char *s)
{
    return append(s, -1);
}

YString &YString::operator+=(const YString &s)
{   
    return append(s, s._len);
}

YString YString::operator+(const char* s)
{
    YString ss = *this;
    ss += s;
    return ss;
}

YString YString::operator+(const YString& s)
{
    YString ss = *this;
    ss += s;
    return ss;
}

YString& YString::append(const char* s, int len)
{
    if (len == -1) len = strlen(s);
    if (_len + len >= _cap) {
        _cap = _len + len + YSTR_SBUF_SIZE - 1;
        auto buf = new char[_cap];
        memcpy(buf, _str, _len);
        if (_str != _sbuf) {
            delete[] _str;
        }
        _str = buf;
    }
    memcpy(_str + _len,s,len);
    _len += len;
    _str[_len] = '\0';
    return *this;
}

YString YString::left(int n) const
{
    if (n > _len) n = _len;
    return YString(_str,n);
}

YString YString::mid(int from, int n) const
{
    if (_len == 0) return YString();
    if (from < 0) from = 0;
    else if (from >= _len) from = _len - 1;
    if (n > _len - from) n = _len - from;
    return YString(&_str[from],n);
}

YString YString::right(int n) const
{
    if (_len == 0) return YString();
    if (n > _len) n = _len;
    return YString(&_str[_len - n],n);
}

void YString::toUpper()
{
    for (int i = 0; i < _len; i++) {
        if (_str[i] >= 'a' && _str[i] <= 'z')
            _str[i] -= ('a' - 'A');
    }
}

void YString::toLower()
{
    for (int i = 0; i < _len; i++) {
        if (_str[i] >= 'A' && _str[i] <= 'Z')
            _str[i] += ('a' - 'A');
    }
}

int64_t YString::toInt() const
{
    return strtoull(_str,NULL,10);
}

double YString::toDouble() const
{
    return strtod(_str,NULL);
}

YString &YString::format(const char *fmt, ...)
{
    va_list va;
    va_start(va,fmt);
    int len = vsnprintf(nullptr,0,fmt,va);
    if (len >= _cap) {
        _cap = len + YSTR_SBUF_SIZE - 1;
        if (_str != _sbuf) {
            delete[] _str;
        }
        _str = new char[_cap];
    }
    vsnprintf(_str,_cap,fmt,va);
    _len = len;
    _str[_len] = '\0';
    va_end(va);
    return *this;
}

int YString::indexOf(const char *str)
{
    auto p = strstr(_str,str);
    if (!p) return -1;
    int idx = p - _str;
    return idx;
}

int YString::indexOfNoCase(const char *str)
{
    if (str == _str) return 0;
    YString s1(*this);
    YString s2(str);
    s1.toUpper();
    s2.toUpper();
    return s1.indexOf(s2.cstr());
}

bool YString::compare(const char *s)
{
    const char *s1 = _str;
    const char *s2 = s;
    for (int i = 0; i < _len; i++,s1++,s2++) {
        if (*s1 == *s2) continue;
        else return false;
    }
    return true;
}

bool YString::compareNoCase(const char *s)
{
    const char *s1 = _str;
    const char *s2 = s;
    for (int i = 0; i < _len; i++,s1++,s2++) {
        if (*s1 == *s2) continue;
        else if (((*s1 >= 'A' && *s1 <= 'Z') && *s1 == *s2 - ('a' - 'A')) || 
            ((*s1 >= 'a' && *s1 <= 'z') && *s1 == *s2 + ('a' - 'A')))
                continue;
        else return false;
    }
    return true;
}

void YString::clear()
{
    _len = 0;
    _str[_len] = '\0';
}

void YString::trime()
{
    const char *s = _str;
    int left = 0,right = 0;
    for (int i = 0; i < _len; i++) {
        if (s[i] == ' ' || s[i] == '\t') {
            left++;
        }
        else break;
    }
    for (int i = _len - 1; i >= 0; i--) {
        if (s[i] == ' ' || s[i] == '\t') {
            right++;
        }
        else break;
    }
    int n = _len - (left + right);
    if (n < _len) {
        for (int i = 0; i < n; i++) {
            _str[i] = _str[left + i];
        }
        _str[n] = '\0';
        _len = n;
    }
}

std::vector<YString> YString::split(const char *token)
{
    std::vector<YString> coll;
    const char *s1 = _str;
    const char *pstart = s1;
    while(*s1) {
        if (strstr(s1,token) == s1) {
            int len = s1 - pstart;
            if (len > 0) {
                YString sub(pstart,len);
                coll.push_back(sub);
            }
            s1++;
            pstart = s1;
        }
        else {
            s1++;
        }
    }
    if (coll.size() > 0 && s1 > pstart) {
        int len = s1 - pstart;
        YString sub(pstart, len);
        coll.push_back(sub);
    }
    return coll;
}

YString &YString::join(const std::vector<YString> &ss, char *s)
{
    for (int i = 0; i < ss.size(); i++) {
        *this += ss[i];
        if (i != ss.size() - 1)
            *this += s;
    }
    return *this;
}
