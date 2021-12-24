INC= ../include ../pico ../clog
INC_PARAMS=$(INC:%=-I%)

SRC= verify cache dispatcher client server main
SRCDIR = ../src/
SRC_PARAMS=$(SRC:%=$(SRCDIR)%.c)

BUILDDIR= build
OBJ_PARAMS=$(SRC:%=%.o) pico.o logger.o

FLAGS= -Wall -Wextra
LFLAGS= -lpthread

all: proxy cleanup

clog:
	gcc -c clog/logger.c -o $(BUILDDIR)/logger.o

pico:
	gcc -c pico/picohttpparser.c -w -o $(BUILDDIR)/pico.o

proxy: pico clog
	mkdir -p $(BUILDDIR); 							 \
	cd $(BUILDDIR); 	  						 \
	gcc $(INC_PARAMS) -c $(SRC_PARAMS) $(FLAGS); \
	gcc -o proxy $(OBJ_PARAMS) $(LFLAGS)

cleanup:
	rm $(OBJ_PARAMS:%=$(BUILDDIR)/%)
