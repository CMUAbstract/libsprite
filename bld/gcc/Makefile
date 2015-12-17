EXEC = libsprite.a

OBJECTS = \
	SpriteRadio.o \
	CC430Radio.o \
	random.o \
	prn.o \

CONFIG_PRINTF_LIB = libmspsoftuart

# Select a pair of indexes of PRN arrays for Gold code communication
#CONFIG_PRN_0 = 2
#CONFIG_PRN_1 = 3

CONFIG_PRN_0 = 266
CONFIG_PRN_1 = 267

#CONFIG_PRN_0 = 268
#CONFIG_PRN_1 = 269

include $(MAKER_ROOT)/Makefile.gcc
include $(MAKER_ROOT)/Makefile.console

# TODO: what cpu frequency are we running at?
CFLAGS += \
	-DF_CPU=8000000 \
	-DCONFIG_PRN_0=$(CONFIG_PRN_0) \
	-DCONFIG_PRN_1=$(CONFIG_PRN_1) \
	-I../../src/include/libsprite \

VPATH = ../../src
