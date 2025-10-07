#ifndef TJ_DATETIME_H

/*
 * sqlite3中的日期函数
 *
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* dt_time(int argc, ...);
const char* dt_datetime(int argc, ...);
const char* dt_date(int argc, ...);
int64_t dt_unixepoch(int argc, ...);
double dt_julianday(int argc, ...);
const char* dt_timediff(int argc, ...);
const char* dt_strftime(const char* zFmt, int argc, ...);

#ifdef __cplusplus
};
#endif

#endif
