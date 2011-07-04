export LD_LIBRARY_PATH=/usr/local/mysql/lib/mysql/
DBNAME=$1
WH=$2
./tpcc_load localhost $DBNAME root "" $WH 1 1 $WH >> 1.out &
./tpcc_load localhost $DBNAME root "" $WH 2 1 $WH  >> 2.out &
./tpcc_load localhost $DBNAME root "" $WH 3 1 $WH  >> 3.out &
./tpcc_load localhost $DBNAME root "" $WH 4 1 $(($WH/2))  >> 4_1.out &
./tpcc_load localhost $DBNAME root "" $WH 4 $(($WH/2+1)) $WH >> 4_2.out &
