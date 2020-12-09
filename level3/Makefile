CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 # compilation flags
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build

# For students 
LIB_UTIL = cd_ls_pwd.o link_unlink.o mkdir_create.o rmdir.o util.o file.o mount_umount.o
SRCS   = main.c cd_ls_pwd.c link_unlink.c mkdir_create.c rmdir.c util.c file.c mount_umount.c
OBJS   = main.o $(LIB_UTIL) 

TARGETS= main 

all: ${TARGETS}

main: $(OBJS) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) 
