
#-------------------------------------------------------
# Creates demo CFS web-service daemon handler
#-------------------------------------------------------

BINDIR = ../bin

install: web-service-handler web-service-proc.o
	gcc -shared -fPIC -Wl,-soname,libweb-service-proc.so -o $(BINDIR)/libweb-service-proc.so $(BINDIR)/web-service-proc.o -lc -lcfsapi 
	sudo cp $(BINDIR)/libweb-service-proc.so /usr/lib/clarasoft
	sudo ldconfig -n /usr/lib/clarasoft
	sudo ln -sf /usr/lib/clarasoft/libweb-service-proc.so /usr/lib/libweb-service-proc.so

web-service-proc.o: web-service-proc.c
	gcc -g -fPIC -Wall -c web-service-proc.c -o $(BINDIR)/web-service-proc.o

web-service-handler: web-service-handler.o
	gcc -g $(BINDIR)/web-service-handler.o -Wall -o $(BINDIR)/web-service-handler -lcfsapi -lcslib -ldl

web-service-handler.o: web-service-handler.c
	gcc -g -Wall -c web-service-handler.c -o $(BINDIR)/web-service-handler.o

clean:
	rm $(BINDIR)/*.o

