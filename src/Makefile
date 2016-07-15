#
# "make all" to build necessary executables.
#



LIBS=		`mysql_config --libs_r` -lrt

INC=		-I. `mysql_config --include`

#DEFS=		-DDEBUG

CFLAGS=		-w -O3 -g

TRANSACTIONS=	neword.o payment.o ordstat.o delivery.o slev.o
OBJS=		main.o spt_proc.o driver.o support.o sequence.o rthist.o sb_percentile.o $(TRANSACTIONS)

.SUFFIXES:
.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(INC) $(DEFS) -c $*.c

all: ../tpcc_load ../tpcc_start

../tpcc_load : load.o support.o
	$(CC) load.o support.o -DTEXT_TEST -DLINUX -DTPCH dbgen/text.c dbgen/bm_utils.c dbgen/rng64.c dbgen/rnd.c dbgen/speed_seed.c dbgen/permute.c dbgen/build.c $(LIBS) -o ../tpcc_load

../tpcc_start : $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o ../tpcc_start

clean :
	rm -f *.o
