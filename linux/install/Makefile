
CC = gcc
CLARASOFT_INCLUDEDIR = /usr/include/clarasoft
CLARASOFT_LIBDIR = /usr/lib/clarasoft
LIBDIR = /usr/lib
BINDIR = ./bin
SRCDIR = ./sources
DEMODIR = ./sources/examples
SQLSRCDIR = ./sources/embedded-SQL
FLAGS = -g -Wall -fPIC

install: stddir stdinclude libcslib libcfsapi clarad clarah csapbrkr 
	rm $(BINDIR)/*.o
	rm $(BINDIR)/*.so

install-with-sql: stddir stdinclude libcfsapi-with-sql clarad clarah csapbrkr
	rm $(BINDIR)/*.o
	rm $(BINDIR)/*.so

update: stdinclude libcslib libcfsapi clarad clarah csapbrkr
	rm $(BINDIR)/*.o
	rm $(BINDIR)/*.so

update-with-sql: stdinclude libcfsapi-with-sql clarad clarah csapbrkr
	rm $(BINDIR)/*.o
	rm $(BINDIR)/*.so

libcfsapi: libcslib cfsrepo.o cfsapi.o cshttp.o cswsck.o csap.o
	$(CC) -shared -fPIC -Wl,-soname,libcfsapi.so -o $(BINDIR)/libcfsapi.so $(BINDIR)/cfsrepo.o $(BINDIR)/cfsapi.o $(BINDIR)/cshttp.o $(BINDIR)/cswsck.o $(BINDIR)/csap.o -lc -lcslib -lssl
	sudo cp $(BINDIR)/libcfsapi.so $(CLARASOFT_LIBDIR)
	sudo ldconfig -n /usr/lib/clarasoft
	sudo ln -sf $(CLARASOFT_LIBDIR)/libcfsapi.so $(LIBDIR)/libcfsapi.so

libcfsapi-with-sql: libcslib cfsrepo-with-sql.o cfsapi.o cshttp.o cswsck.o csap.o
    #########################################################################################
    # For PostgreSQL embedded SQL library
    #   $(CC) -shared -fPIC -Wl,-soname,libcfsapi.so -o $(BINDIR)/libcfsapi.so $(BINDIR)/cfsrepo.o $(BINDIR)/cfsapi.o $(BINDIR)/cshttp.o $(BINDIR)/cswsck.o $(BINDIR)/csap.o -lc -lcslib -L/usr/postgresql/lib -lecpg -lssl
    #########################################################################################
    # For PostgreSQL native C interface:
    #########################################################################################
	$(CC) -shared -fPIC -Wl,-soname,libcfsapi.so -o $(BINDIR)/libcfsapi.so $(BINDIR)/cfsrepo.o $(BINDIR)/cfsapi.o $(BINDIR)/cshttp.o $(BINDIR)/cswsck.o $(BINDIR)/csap.o -lc -lcslib -lssl -lpq
	sudo cp $(BINDIR)/libcfsapi.so $(CLARASOFT_LIBDIR)
	sudo ldconfig -n /usr/lib/clarasoft
	sudo ln -sf $(CLARASOFT_LIBDIR)/libcfsapi.so $(LIBDIR)/libcfsapi.

csapbrkr: csapbrkr.o
	$(CC) -Wall $(BINDIR)/csapbrkr.o -lcfsapi -lcslib -luuid -ldl -lcrypt -o $(BINDIR)/csapbrkr
	sudo cp $(BINDIR)/csapbrkr $(CLARASOFT_LIBDIR)

csapbrkr.o: $(SRCDIR)/csapbrkr.c
	$(CC) $(FLAGS) -c $(SRCDIR)/csapbrkr.c -o $(BINDIR)/csapbrkr.o

csap.o: $(SRCDIR)/csap.c
	gcc $(FLAGS) -c $(SRCDIR)/csap.c -o $(BINDIR)/csap.o

cswsck.o: $(SRCDIR)/cswsck.c
	$(CC) $(FLAGS) -c $(SRCDIR)/cswsck.c -o $(BINDIR)/cswsck.o

cshttp.o: $(SRCDIR)/cshttp.c
	$(CC) $(FLAGS) -c $(SRCDIR)/cshttp.c -o $(BINDIR)/cshttp.o

cfsapi.o: $(SRCDIR)/cfsapi.c
	$(CC) $(FLAGS) -c $(SRCDIR)/cfsapi.c -o $(BINDIR)/cfsapi.o

cfsrepo.o: $(SRCDIR)/cfsrepo.c
	$(CC) $(FLAGS) -c $(SRCDIR)/cfsrepo.c -o $(BINDIR)/cfsrepo.o

cfsrepo-with-sql.o: $(SRCDIR)/cfsrepo.c
	$(CC) $(FLAGS) -D__CLARASOFT_CFS_POSTGRESQL_SUPPORT="" -c $(SRCDIR)/cfsrepo.c -o $(BINDIR)/cfsrepo.o

clarad: clarad.o 
	$(CC) $(FLAGS) $(BINDIR)/clarad.o -o $(BINDIR)/clarad -lcfsapi -lcslib -ldl
	sudo cp $(BINDIR)/clarad $(CLARASOFT_LIBDIR)

clarad.o: $(SRCDIR)/clarad.c
	$(CC) $(FLAGS) -c $(SRCDIR)/clarad.c -o $(BINDIR)/clarad.o

clarah: clarah.o 
	$(CC) $(FLAGS) $(BINDIR)/clarah.o -o $(BINDIR)/clarah -lcfsapi
	sudo cp $(BINDIR)/clarah $(CLARASOFT_LIBDIR)

clarah.o: $(SRCDIR)/clarah.c
	$(CC) $(FLAGS) -c $(SRCDIR)/clarah.c -o $(BINDIR)/clarah.o

libcslib: cslib.o csjson.o
	$(CC) -shared -fPIC -Wl,-soname,libcslib.so -o $(BINDIR)/libcslib.so $(BINDIR)/cslib.o $(BINDIR)/csjson.o -lc -luuid
	sudo cp $(BINDIR)/libcslib.so $(CLARASOFT_LIBDIR)
	sudo ldconfig -n $(CLARASOFT_LIBDIR)
	sudo ln -sf $(CLARASOFT_LIBDIR)/libcslib.so $(LIBDIR)/libcslib.so

csjson.o: $(SRCDIR)/csjson.c
	$(CC) $(FLAGS) -c $(SRCDIR)/csjson.c -o $(BINDIR)/csjson.o
	
cslib.o: $(SRCDIR)/cslib.c
	$(CC) $(FLAGS) -c $(SRCDIR)/cslib.c -o $(BINDIR)/cslib.o

stdinclude:
	cp $(SRCDIR)/cslib.h $(CLARASOFT_INCLUDEDIR)		
	cp $(SRCDIR)/csjson.h $(CLARASOFT_INCLUDEDIR)		
	cp $(SRCDIR)/cfsapi.h $(CLARASOFT_INCLUDEDIR)
	cp $(SRCDIR)/cfsrepo.h $(CLARASOFT_INCLUDEDIR)
	cp $(SRCDIR)/csap.h $(CLARASOFT_INCLUDEDIR)
	cp $(SRCDIR)/cshttp.h $(CLARASOFT_INCLUDEDIR)		
	cp $(SRCDIR)/cswsck.h $(CLARASOFT_INCLUDEDIR)		
	cp $(SRCDIR)/cfs.h $(CLARASOFT_INCLUDEDIR)		

stddir:
	mkdir $(CLARASOFT_LIBDIR)
	mkdir $(CLARASOFT_INCLUDEDIR)



