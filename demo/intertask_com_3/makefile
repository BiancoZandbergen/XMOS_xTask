# Makefile for the FreeRTOS XMOS XS1 port.
# Works on GNU/Linux and Mac OS X.
# Might need modification on Windows.

# Uncomment a pair of variables to select the target
# The BOARD variable is mainly used to configure the LEDs
#
#BOARD=XC_1
#TARGET=XS1-G04B-FB512-C4
#
#BOARD=XC_1A
#TARGET=XC-1A
#
#BOARD=XC_2
#TARGET=XC-2
#
#BOARD=XK_1
#TARGET=XS1-L8A-64-TQ128-C5
#
#BOARD=XK_1A
#TARGET=XK-1A
#
#BOARD=XDK
#TARGET=XS1-G04B-FB512-C4
#
BOARD=STARTKIT
TARGET=STARTKIT
#
#

# program executable name
PROGRAM=demo.xe

# compiler optimisation
SRC_OPT=-O0

# debug options
DEBUG=-g

SOURCE_DIR=../../xtask/src
INCLUDE_DIR=../../xtask/include
AP_DIR=.
INCLUDE=-I . -I $(SOURCE_DIR)/include
INCLUDE=
CFLAGS= $(DEBUG) -Wall -target=$(TARGET)  $(INCLUDE)

REMOVE=rm -f

# Operating System objects
OBJS=kernel.o kernel_asm.o task.o kcalls.o comserver.o comserver_asm.o

# Application objects
OBJS+= ap.o main.o

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	xcc -report $(CFLAGS)  $(OBJS) -o $(PROGRAM)

kernel.o: $(SOURCE_DIR)/kernel.c
	xcc -c $(SRC_OPT) $(CFLAGS)  $(SOURCE_DIR)/kernel.c

kernel_asm.o: $(SOURCE_DIR)/kernel_asm.S
	xcc -c $(SRC_OPT) $(CFLAGS) $(SOURCE_DIR)/kernel_asm.S

task.o: $(SOURCE_DIR)/task.c
	xcc -c $(SRC_OPT) $(CFLAGS) $(SOURCE_DIR)/task.c

kcalls.o: $(SOURCE_DIR)/kcalls.c
	xcc -c $(SRC_OPT) $(CFLAGS) $(SOURCE_DIR)/kcalls.c

comserver.o: $(SOURCE_DIR)/comserver.c
	xcc -c $(SRC_OPT) $(CFLAGS) $(SOURCE_DIR)/comserver.c

comserver_asm.o: $(SOURCE_DIR)/comserver_asm.S
	xcc -c $(SRC_OPT) $(CFLAGS) $(SOURCE_DIR)/comserver_asm.S

# Application

main.o: main.xc
	xcc -c $(SRC_OPT) $(CFLAGS) -D $(BOARD) main.xc

ap.o: ap.c
	xcc -c $(SRC_OPT) $(CFLAGS) ap.c

clean:
	$(REMOVE) $(OBJS) $(PROGRAM)

run:
	xrun --io $(PROGRAM)

runsim:
	xsim -t $(PROGRAM)
