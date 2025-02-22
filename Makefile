libdatetime.so: datetime.c datetime.h
	gcc -g -O2 -Wall -fPIC --shared -o $@ $<