#  Project Name
PROJECT=tbdm

#  Type of CPU/MCU in target hardware
CPU = cortex-m4

TCPREFIX=arm-none-eabi-

CC=$(TCPREFIX)gcc
LD=$(TCPREFIX)ld
CPP=$(TCPREFIX)cpp
OBJCOPY=$(TCPREFIX)objcopy
AR=$(TCPREFIX)ar
RANLIB=$(TCPREFIX)ranlib

INCLUDE=-Iinclude
CFLAGS=-Wall \
	-fno-common \
	-mcpu=$(CPU) \
	-mthumb \
	-O2

LDFLAGS=

TRGTDIRS= ./teensy
OBJDIRS=$(patsubst %, %/objs,$(TRGTDIRS))

VPATH= sys src util

LDCFILE=tbdm.lk
LDCSRC=tbdm.lk.in

LIBTEENSY=libteensy.a
LDLIBS=$(patsubst %,%/$(LIBTEENSY),$(TRGTDIRS))

FLASH_EXEC=tbdm.hex


CSRCS= \
	tbdm.c \
	tbdm_main.c \
	sysinit.c \
	uart.c \
	bdmcf.c \
	cmd_processing.c \
	xprintf.c \
	xstring.c \
	wait.c \
	arm_cm4.c


teensy/objs/wait.o: CFLAGS += -O4

ASRCS= \
	crt0.S

SRCS=$(ASRCS) $(CSRCS)
COBJS=$(patsubst %.c,%.o,$(CSRCS))
AOBJS=$(patsubst %.S,%.o,$(ASRCS))

OBJS=$(COBJS) $(AOBJS)

all: $(patsubst %,%/$(FLASH_EXEC),$(TRGTDIRS))

.PHONY: clean
clean:
	for d in $(TRGTDIRS); \
		do rm -f $$d/*.map $$d/*.hex $$d/*.elf $$d/*.lk $$d/objs/* $$d/depend; \
	done
	rm -f tags


#
# generate pattern rules for libraries
#

define AR_TEMPLATE
$(1)_OBJS=$(patsubst %,$(1)/objs/%,$(OBJS))
$(1)/$(LIBTEENSY): $$($(1)_OBJS)
	$(AR) rv $$@ $$?
	$(RANLIB) $$@
endef
$(foreach DIR,$(TRGTDIRS),$(eval $(call AR_TEMPLATE,$(DIR))))

#
# generate pattern rules for different object files
#
define CC_TEMPLATE
$(1)/objs/%.o:%.c
	$(CC) $$(CFLAGS) $(INCLUDE) -c $$< -o $$@

$(1)/objs/%.o:%.S
	$(CC) $$(CFLAGS) $(INCLUDE) -c $$< -o $$@

endef
$(foreach DIR,$(TRGTDIRS),$(eval $(call CC_TEMPLATE,$(DIR))))

#
# rules for depend
#
define DEP_TEMPLATE
ifneq (clean,$$(MAKECMDGOALS))
include $(1)/depend
endif

$(1)/depend:$(SRCS)
	$(CC) $$(CFLAGS) $(INCLUDE) -M $$^ | sed -e "s#^\(.*\).o:#"$(1)"/objs/\1.o:#" > $$@

endef
$(foreach DIR,$(TRGTDIRS),$(eval $(call DEP_TEMPLATE,$(DIR))))

define EX_TEMPLATE
# pattern rule for flash
$(1)_MAPFILE=$(1)/$$(basename $$(FLASH_EXEC)).map
$(1)_OBJS=$(patsubst %,$(1)/objs/%,$(OBJS))
$(1)/$$(FLASH_EXEC): $(1)/$(LIBTEENSY) $(LDCSRC)
	$(CPP) $(INCLUDE) -DOBJDIR=$(1)/objs -P $(LDCSRC) -o $(1)/$$(LDCFILE)
	$(CC) -nostartfiles -mthumb -Wl,-Map -Wl,$$($(1)_MAPFILE) -Wl,--cref -Wl,-T -Wl,$(1)/$$(LDCFILE) $$($(1)_OBJS) -o $$(basename $$@).elf
	$(OBJCOPY) -O ihex -R .stack $$(basename $$@).elf $$@
endef
$(foreach DIR,$(TRGTDIRS),$(eval $(call EX_TEMPLATE,$(DIR))))


.PHONY: printvars
printvars:
	@$(foreach V,$(.VARIABLES), $(if $(filter-out environment% default automatic, $(origin $V)),$(warning $V=$($V))))
