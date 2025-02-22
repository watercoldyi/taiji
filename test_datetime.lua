local ffi = require "ffi"

ffi.cdef [[
    const char *dt_time(int argc, ...);
    const char *dt_datetime(int argc, ...);
    const char *dt_date(int argc, ...);
    int64_t dt_unixepoch(int argc, ...)
    double dt_julianday(int argc, ...);
    const char *dt_timediff(int argc, ...);
    const char *dt_strftime(const char *zFmt, int argc, ...);
]]

local S = ffi.load("datetime.so")

print(ffi.string(S.dt_datetime(2,"now","+1 days","localtime")))
print(ffi.string(S.dt_date(2,"now","+1 days","localtime")))
print(ffi.string(S.dt_time(2,"now","+1 days","localtime")))
print(S.dt_julianday(1,"2013-01-01 00:30"))
print(S.dt_unixepoch(2,"now","+1 days","localtime"))
print(ffi.string(S.dt_timediff(2,"now","2013-01-01")))
print(ffi.string(S.dt_strftime("小时 %H",3,"now","localtime","+2 hours")))

