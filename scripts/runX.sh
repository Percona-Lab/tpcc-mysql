#!/bin/sh
set -u
set -x
set -e

#export LD_LIBRARY_PATH=/usr/local/mysql/lib/mysql/
export LD_LIBRARY_PATH=/usr/local/Percona-Server/lib/mysql/
MYSQLDIR=/usr/local/mysql

ulimit -c unlimited

#DR="/mnt/fio320"
BD=/mnt/fio160/back/tpc1000w
#DR=/data/db/bench
DR="/mnt/tachion/tpc1000w"
CONFIG="/etc/my.y.cnf"
#CONFIG="/etc/my.y.557.cnf"

WT=10
RT=10800

ROWS=80000000

log2="/data/bench/"
#log2="$DR/"

# restore from backup
function restore {

mkdir -p $DR

rm -fr $DR/*
rm -f $log2/ib_log*

echo $log2
#for nm in ibdata1 ib_logfile0 ib_logfile1
for nm in ibdata1 
do
rm -f $log2/$nm
pagecache-management.sh cp $BD/$nm $log2
done


cp -r $BD/mysql $DR
pagecache-management.sh cp -r $BD/tpcc1000 $DR
pagecache-management.sh cp -r $BD/ibdata1 $log2

sync
echo 3 > /proc/sys/vm/drop_caches

chown mysql.mysql -R $DR
chown mysql.mysql -R $log2
}


function waitm {

while [ true ]
do

mysql -e "set global innodb_max_dirty_pages_pct=0" mysql

wt=`mysql -e "SHOW ENGINE INNODB STATUS\G" | grep "Modified db pages" | sort -u | awk '{print $4}'`
if [[ "$wt" -lt 100 ]] ;
then
mysql -e "set global innodb_max_dirty_pages_pct=90" mysql
break
fi

echo "mysql pages $wt"
sleep 10
done

}



# Determine run number for selecting an output directory
RUN_NUMBER=-1

if [ -f ".run_number" ]; then
  read RUN_NUMBER < .run_number
fi

if [ $RUN_NUMBER -eq -1 ]; then
        RUN_NUMBER=0
fi

OUTDIR=res$RUN_NUMBER
mkdir -p $OUTDIR

RUN_NUMBER=`expr $RUN_NUMBER + 1`
echo $RUN_NUMBER > .run_number

#for par in  1 2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 43
#for par in  13 26 39 52 65 78
#for par in 39 52 65 78
#for par in 13 52 78 144
for par in 52 144
do

restore

export OS_FILE_LOG_BLOCK_SIZE=4096
#/home/yasufumi/opt/mysql-5.5.6_p/bin/mysqld --defaults-file=$CONFIG --datadir=$DR  --innodb_log_group_home_dir=$log2 --innodb_thread_concurrency=0 --innodb-buffer-pool-size=${par}GB &
/usr/local/mysql/libexec/mysqld --defaults-file=$CONFIG --datadir=$DR --innodb_data_home_dir=$log2 --innodb_log_group_home_dir=$log2 --innodb_thread_concurrency=0 --innodb-buffer-pool-size=${par}GB &
#/usr/local/mysql/bin/mysqld --defaults-file=$CONFIG --datadir=$DR  --innodb_log_group_home_dir=$log2 --innodb_thread_concurrency=0 --innodb-buffer-pool-size=${par}GB --innodb-buffer-pool-instances=8 &

MYSQLPID=$!

set +e

while true;
do
mysql -Bse "SELECT 1" mysql

if [ "$?" -eq 0 ]
then
  break
fi

sleep 30

echo -n "."
done
set -e




iostat -dx 5 2000 >> $OUTDIR/iostat.${par}res &
PID=$!
vmstat 5 2000 >> $OUTDIR/vmstat.${par}res &
PIDV=$!
./virident_stat.sh >> $OUTDIR/virident.${par}res &
PIDVS=$!
$MYSQLDIR/bin/mysqladmin ext -i10 -r >> $OUTDIR/mysqladminext.${par}res &
PIDMYSQLSTAT=$!


cp $CONFIG $OUTDIR
cp $0 $OUTDIR
mysqladmin variables >>  $OUTDIR/mysql_variables.res
./tpcc_start localhost tpcc1000 root "" 1000 24 10 3600 | tee -a $OUTDIR/tpcc.${par}.out
kill $PIDMYSQLSTAT
kill -9 $PID
kill -9 $PIDV
kill -9 $PIDVS
kill -9 $MYSQLPID

#waitm


#mysqladmin  shutdown


done
