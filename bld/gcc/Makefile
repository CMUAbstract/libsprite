EXEC = libsprite.a

OBJECTS = \
	radio.o \
	random.o \

CONFIG_PRINTF_LIB = libmspsoftuart

include $(MAKER_ROOT)/Makefile.gcc
include $(MAKER_ROOT)/Makefile.console

# TODO: what cpu frequency are we running at?
CFLAGS += \
	-DF_CPU=8000000 \
	-I../../src/include/libsprite \

VPATH = ../../src
