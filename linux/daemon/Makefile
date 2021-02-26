
BINDIR = ../bin

clarad: clarad.o
	gcc -g $(BINDIR)/clarad.o -Wall -o $(BINDIR)/clarad -lcfsapi -lcslib -ldl

clarad.o: clarad.c
	gcc -g -c clarad.c -Wall -o $(BINDIR)/clarad.o

clean:
	rm $(BINDIR)/*.o
  
