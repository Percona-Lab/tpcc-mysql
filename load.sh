export LD_LIBRARY_PATH=/usr/local/mysql/lib/mysql/
DBNAME=$1
WH=$2
HOST=localhost
STEP=100

./tpcc_load $HOST $DBNAME root "" $WH 1 1 $WH >> 1.out &

x=1

while [ $x -le $WH ]
do
 echo $x $(( $x + $STEP - 1 ))
./tpcc_load $HOST $DBNAME root "" $WH 2 $x $(( $x + $STEP - 1 ))  >> 2_$x.out &
./tpcc_load $HOST $DBNAME root "" $WH 3 $x $(( $x + $STEP - 1 ))  >> 3_$x.out &
./tpcc_load $HOST $DBNAME root "" $WH 4 $x $(( $x + $STEP - 1 ))  >> 4_$x.out &
 x=$(( $x + $STEP ))
done

