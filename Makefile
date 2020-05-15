CC= 		gcc
CFLAGS=		-g -Wall -Werror -std=gnu99 -Iinclude
LD=		gcc
LDFLAGS=	-L.
AR=		ar
ARFLAGS=	rcs
TARGETS=	bin/main

all:		$(TARGETS)

clean:
	@echo Cleaning...
	@rm -f $(TARGETS) lib/*.a src/*.o *.log *.input

.PHONY:		all test clean

src/%.o:	src/%.c
	@echo Compiling $@...
	@$(CC) $(CFLAGS) -c $^ -o $@

lib/libmain.a:	src/forking.o src/handler.o src/request.o src/single.o src/socket.o src/utils.o
	@echo Linking $@...
	@$(AR) $(ARFLAGS) $@ $^

bin/main:	src/main.o lib/libmain.a
	@echo Linking $@...
	@$(LD) $(LDFLAGS) $^ -o $@ 
