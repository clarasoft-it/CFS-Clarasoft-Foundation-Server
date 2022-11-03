
CC = gcc
CLARASOFT_INCLUDEDIR = /usr/include/clarasoft
CLARASOFT_LIBDIR = /usr/lib/clarasoft
LIBDIR = /usr/lib
BINDIR = ./bin
SRCDIR = ./sources
EXAMPLES_SRCDIR = ./sources/examples
SQLSRCDIR = ./sources/embedded-SQL
FLAGS = -g -Wall -fPIC

install: stddir stdinclude libcslib libcfsapi clarad clarah websckh csapbrkr 
	rm $(BINDIR)/*.o

install-with-sql: stddir stdinclude libcfsapi-with-sql clarad clarah websckh csapbrkr
	rm $(BINDIR)/*.o

update: stdinclude libcslib libcfsapi clarad clarah websckh csapbrkr
	rm $(BINDIR)/*.o

update-with-sql: stdinclude libcfsapi-with-sql clarad clarah websckh csapbrkr
	rm $(BINDIR)/*.o

examples: libbasic-csap-service libbasic-echo-service libbasic-websocket-service basic-csap-client basic-echo-client basic-websocket-client basic-http-client
	rm $(BINDIR)/*.o

libcfsapi: libcslib cfsrepo.o cfsapi.o cshttp.o cswsck.o csap.o
	$(CC) -shared -fPIC -Wl,-soname,libcfsapi.so -o $(CLARASOFT_LIBDIR)/libcfsapi.so $(BINDIR)/cfsrepo.o $(BINDIR)/cfsapi.o $(BINDIR)/cshttp.o $(BINDIR)/cswsck.o $(BINDIR)/csap.o -lc -lcslib -lssl
	ldconfig -n /usr/lib/clarasoft
	ln -sf $(CLARASOFT_LIBDIR)/libcfsapi.so $(LIBDIR)/libcfsapi.so

libcfsapi-with-sql: libcslib cfsrepo-with-sql.o cfsapi.o cshttp.o cswsck.o csap.o
    #########################################################################################
    # For PostgreSQL embedded SQL library
    #   $(CC) -shared -fPIC -Wl,-soname,libcfsapi.so -o $(BINDIR)/libcfsapi.so $(BINDIR)/cfsrepo.o $(BINDIR)/cfsapi.o $(BINDIR)/cshttp.o $(BINDIR)/cswsck.o $(BINDIR)/csap.o -lc -lcslib -L/usr/postgresql/lib -lecpg -lssl
    #########################################################################################
    # For PostgreSQL native C interface:
    #########################################################################################
	$(CC) -shared -fPIC -Wl,-soname,libcfsapi.so -o $(CLARASOFT_LIBDIR)/libcfsapi.so $(BINDIR)/cfsrepo.o $(BINDIR)/cfsapi.o $(BINDIR)/cshttp.o $(BINDIR)/cswsck.o $(BINDIR)/csap.o -lc -lcslib -lssl -lpq
	ldconfig -n /usr/lib/clarasoft
	ln -sf $(CLARASOFT_LIBDIR)/libcfsapi.so $(LIBDIR)/libcfsapi

libbasic-csap-service: basic-csap-service.o
	$(CC) -shared -fPIC -Wl,-soname,libbasic-csap-service.so -o $(CLARASOFT_LIBDIR)/libbasic-csap-service.so $(BINDIR)/basic-csap-service.o -lc -lcfsapi
	ldconfig -n /usr/lib/clarasoft
	ln -sf $(CLARASOFT_LIBDIR)/libbasic-csap-service.so $(LIBDIR)/libbasic-csap-service.so

libbasic-echo-service: basic-echo-service.o
	$(CC) -shared -fPIC -Wl,-soname,libbasic-echo-service.so -o $(CLARASOFT_LIBDIR)/libbasic-echo-service.so $(BINDIR)/basic-echo-service.o -lc -lcfsapi
	ldconfig -n /usr/lib/clarasoft
	ln -sf $(CLARASOFT_LIBDIR)/libbasic-echo-service.so $(LIBDIR)/libbasic-echo-service.so

libbasic-websocket-service: basic-websocket-service.o
	$(CC) -shared -fPIC -Wl,-soname,libbasic-websocket-service.so -o $(CLARASOFT_LIBDIR)/libbasic-websocket-service.so $(BINDIR)/basic-websocket-service.o -lc -lcfsapi
	ldconfig -n /usr/lib/clarasoft
	ln -sf $(CLARASOFT_LIBDIR)/libbasic-websocket-service.so $(LIBDIR)/libbasic-websocket-service.so

csapbrkr: csapbrkr.o
	$(CC) -Wall $(BINDIR)/csapbrkr.o -lcfsapi -lcslib -luuid -ldl -lcrypt -o $(CLARASOFT_LIBDIR)/csapbrkr

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
	$(CC) $(FLAGS) $(BINDIR)/clarah.o -o $(CLARASOFT_LIBDIR)/clarah -lcfsapi

clarah.o: $(SRCDIR)/clarah.c
	$(CC) $(FLAGS) -c $(SRCDIR)/clarah.c -o $(BINDIR)/clarah.o

websckh: websckh.o 
	$(CC) $(FLAGS) $(BINDIR)/websckh.o -o $(CLARASOFT_LIBDIR)/websckh -lcfsapi

websckh.o: $(SRCDIR)/websckh.c
	$(CC) $(FLAGS) -c $(SRCDIR)/websckh.c -o $(BINDIR)/websckh.o

basic-csap-client: basic-csap-client.o
	$(CC) $(FLAGS) $(BINDIR)/basic-csap-client.o -o $(BINDIR)/basic-csap-client -lcfsapi

basic-csap-client.o: $(EXAMPLES_SRCDIR)/basic-csap-client.c
	$(CC) $(FLAGS) -c $(EXAMPLES_SRCDIR)/basic-csap-client.c -o $(BINDIR)/basic-csap-client.o

basic-echo-client: basic-echo-client.o
	$(CC) $(FLAGS) $(BINDIR)/basic-echo-client.o -o $(BINDIR)/basic-echo-client -lcfsapi

basic-echo-client.o: $(EXAMPLES_SRCDIR)/basic-echo-client.c
	$(CC) $(FLAGS) -c $(EXAMPLES_SRCDIR)/basic-echo-client.c -o $(BINDIR)/basic-echo-client.o

basic-http-client: basic-http-client.o
	$(CC) $(FLAGS) $(BINDIR)/basic-http-client.o -o $(BINDIR)/basic-http-client -lcfsapi

basic-http-client.o: $(EXAMPLES_SRCDIR)/basic-http-client.c
	$(CC) $(FLAGS) -c $(EXAMPLES_SRCDIR)/basic-http-client.c -o $(BINDIR)/basic-http-client.o

basic-websocket-client: basic-websocket-client.o
	$(CC) $(FLAGS) $(BINDIR)/basic-websocket-client.o -o $(BINDIR)/basic-websocket-client -lcfsapi

basic-websocket-client.o: $(EXAMPLES_SRCDIR)/basic-websocket-client.c
	$(CC) $(FLAGS) -c $(EXAMPLES_SRCDIR)/basic-websocket-client.c -o $(BINDIR)/basic-websocket-client.o

basic-csap-service.o: $(EXAMPLES_SRCDIR)/basic-csap-service.c
	$(CC) $(FLAGS) -c $(EXAMPLES_SRCDIR)/basic-csap-service.c -o $(BINDIR)/basic-csap-service.o

basic-echo-service.o: $(EXAMPLES_SRCDIR)/basic-echo-service.c
	$(CC) $(FLAGS) -c $(EXAMPLES_SRCDIR)/basic-echo-service.c -o $(BINDIR)/basic-echo-service.o

basic-websocket-service.o: $(EXAMPLES_SRCDIR)/basic-websocket-service.c
	$(CC) $(FLAGS) -c $(EXAMPLES_SRCDIR)/basic-websocket-service.c -o $(BINDIR)/basic-websocket-service.o

libcslib: cslib.o csjson.o
	$(CC) -shared -fPIC -Wl,-soname,libcslib.so -o $(CLARASOFT_LIBDIR)/libcslib.so $(BINDIR)/cslib.o $(BINDIR)/csjson.o -lc -luuid
	ldconfig -n $(CLARASOFT_LIBDIR)
	ln -sf $(CLARASOFT_LIBDIR)/libcslib.so $(LIBDIR)/libcslib.so

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



