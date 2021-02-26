
CC = gcc
BINDIR = ../bin
FLAGS = -g -Wall -fPIC

driver:
	$(CC) -g driver.c -o ../bin/driver -lcslib -lcfsapi

install: libcfsapi libcslib
	sudo cp $(BINDIR)/libcslib.so /usr/lib/clarasoft
	sudo cp $(BINDIR)/libcfsapi.so /usr/lib/clarasoft
	sudo ldconfig -n /usr/lib/clarasoft
	sudo ln -sf /usr/lib/clarasoft/libcfsapi.so /usr/lib/libcfsapi.so
	sudo ln -sf /usr/lib/clarasoft/libcslib.so /usr/lib/libcslib.so
	
libcfsapi: cfsrepo.o cfsapi.o cshttp.o
	$(CC) -shared -fPIC -Wl,-soname,libcfsapi.so -o $(BINDIR)/libcfsapi.so $(BINDIR)/cfsrepo.o $(BINDIR)/cfsapi.o $(BINDIR)/cshttp.o -lc -lcslib -L/usr/postgresql/lib -lecpg -lssl

cshttp.o: cshttp.c cfsapi.h
	$(CC) $(FLAGS) -I/usr/include/clarasoft -c cshttp.c -o $(BINDIR)/cshttp.o

cfsapi.o: cfsrepo.o cfsrepo.h
	$(CC) $(FLAGS) -I/usr/include/clarasoft -c cfsapi.c -o $(BINDIR)/cfsapi.o

cfsrepo.o: cfsrepo.c cslib.h
	$(CC) $(FLAGS) -I/usr/include/postgresql -I/usr/include/clarasoft -c cfsrepo.c -o ../bin/cfsrepo.o

libcslib: cslib.o csjson.o
	$(CC) -shared -fPIC -Wl,-soname,libcslib.so -o $(BINDIR)/libcslib.so $(BINDIR)/cslib.o $(BINDIR)/csjson.o -lc -luuid

csjson.o: cslib.h
	$(CC) $(FLAGS) -c csjson.c -o $(BINDIR)/csjson.o
	
cslib.o: cslib.c 
	$(CC) $(FLAGS) -c cslib.c -o $(BINDIR)/cslib.o

#################################################################################
# This will precompile a POSTGRE SQL embedded SQL source into a C source file. 
# Change this target if you need to generate cfsrepo.c for another dbms 
#################################################################################

cfsrepo.c: ./embedded-postgre-sql/cfsrepo.pgc
	ecpg ./embedded-postgre-sql/cfsrepo.pgc -o ./cfsrepo.c

#################################################################################
# Cleanup the mess 
#################################################################################

clean:
		rm  $(BINDIR)/*.o