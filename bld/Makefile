LIB = libsprite

OBJECTS = \
	SpriteRadio.o \
	CC430Radio.o \
	random.o \
	prn.o \

override CFLAGS += \
	-I$(SRC_ROOT)/include/$(LIB) \

override SRC_ROOT = ../../src

include ../Makefile.config
include ../Makefile.options
