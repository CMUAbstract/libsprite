EXEC = libsprite.a

OBJECTS = \
	radio.o \

include $(MAKER_ROOT)/Makefile.gcc

# TODO: what cpu frequency are we running at?
CFLAGS += \
	-DF_CPU=16000000 \
	-I../../src/include/libsprite \

VPATH = ../../src
