EXE=app
SRCS=$(wildcard *[a-z].c)

ifeq ($(TARGET), host)
CC=gcc
LD=gcc
STRIP=strip
CFLAGS=-W -Werror -Wpedantic -Wall -Wextra -g -c -O0 -MD -std=gnu99 -DDEBUG
OBJDIR=.obj/host
EXEC=host_$(EXE)

else
# Include the Armadeus APF27 environment variables 
include /home/csel/toolchain/armadeus_env.sh

#APF27 standard makefile for LMI labs
CC=$(ARMADEUS_TOOLCHAIN_PATH)/arm-linux-gcc
LD=$(ARMADEUS_TOOLCHAIN_PATH)/arm-linux-gcc
STRIP=$(ARMADEUS_TOOLCHAIN_PATH)/arm-linux-strip
CFLAGS=-W -Werror -pedantic  -Wall -Wextra -g -c -mcpu=arm926ej-s -O0 -MD -std=gnu99
OBJDIR=.obj/apf27
EXEC=apf27_$(EXE)
endif

OBJS= $(addprefix $(OBJDIR)/, $(ASRC:.s=.o) $(SRCS:.c=.o))

all: $(OBJDIR)/ $(EXEC)
	
$(EXEC): $(OBJS) $(LINKER_SCRIPT)
	@printf 'LD  %-20s ->  %-20s\n' $(OBJS) $@_s
	@$(LD) $(OBJS) $(LDFLAGS) -o $@_s
	@printf 'ST  %-20s ->  %-20s\n' $@_s $@
	@$(STRIP) -g -o $@ $@_s 

$(OBJDIR)/%o: %c
	@printf 'CC  %-20s ->  %-20s\n' $< $@
	@$(CC) $(CFLAGS) $< -o $@
	
$(OBJDIR)/:
	@mkdir -p $(OBJDIR)

clean:
	rm -Rf $(OBJDIR) $(EXEC) $(EXEC)_s

clean_all: 
	rm -Rf .obj apf27_$(EXE) host_$(EXE) apf27_$(EXE)_s host_$(EXE)_s core


.PHONY: all clean clean_all

-include $(OBJS:.o=.d)

