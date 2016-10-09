KERN_SOURCE = $(wildcard *.S)
KERN_SOURCE += $(wildcard *.c)

KERN_OBJ_PATH = $(OBJPATH_ROOT)/kernel

KERN_OBJS_1 = $(patsubst %.S, $(KERN_OBJ_PATH)/%.o, $(KERN_SOURCE))
KERN_OBJS = $(patsubst %.c, $(KERN_OBJ_PATH)/%.o, $(KERN_OBJS_1))

KERN_CFLAGS := $(CFLAGS) -DJOS_KERNEL -gstabs

# How to build kernel object files
$(OBJPATH_ROOT)/kernel/%.o: %.c
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(KERN_CFLAGS) -c -o $@ $<

$(OBJPATH_ROOT)/kernel/%.o: %.S
	@echo + as $<
	@mkdir -p $(@D)
	$(V)$(CC) -nostdinc -D__ASSEMBLY__ $(KERN_CFLAGS) -c -o $@ $<

all: $(KERN_OBJS)
	@echo The source files
	@echo $(KERN_SOURCE)
	@echo The dest files
	@echo $(KERN_OBJS)

