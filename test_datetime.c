#include "datetime.h"
#include <stdio.h>
#include <time.h>
#include <assert.h>

int main()
{
    assert(dt_unixepoch(1,"now") == time(NULL) && "unixepoch invalid");
	assert(dt_julianday(1, "2000-01-01") == 2451544.5);
	assert(dt_julianday(1, "1970-01-01") == 2440587.5);
	assert(dt_julianday(1, "1910-04-20") == 2418781.5);
	assert(dt_julianday(1, "abc") == 0.0);
	return 0;
}