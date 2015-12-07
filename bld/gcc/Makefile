EXEC = libsprite.a

OBJECTS = \
	radio.o \
	random.o \

include $(MAKER_ROOT)/Makefile.gcc

# TODO: what cpu frequency are we running at?
CFLAGS += \
	-DF_CPU=8000000 \
	-I../../src/include/libsprite \

VPATH = ../../src
