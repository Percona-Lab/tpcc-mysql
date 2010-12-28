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
DR="/data/tpc1000w"
#DR="/mnt/tachion/tpc1000w"
CONFIG="/etc/my.y.cnf"
#CONFIG="/etc/my.y.558.cnf"

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
#for nm in ibdata1 
#do
#rm -f $log2/$nm
#pagecache-management.sh cp $BD/$nm $log2
#done


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

for trxv in 2
do
for logsz in 2000M
do
#for par in  1 2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 43
#for par in  13 26 39 52 65 78
#for par in 39 52 65 78
#for par in 13 52 78 144
for par in 13 52 144
do

runid="par$par.log$logsz.trx$trxv."

restore

export OS_FILE_LOG_BLOCK_SIZE=4096
#/usr/local/mysql/bin/mysqld --defaults-file=$CONFIG --datadir=$DR --innodb_data_home_dir=$DR --innodb_log_group_home_dir=$DR --innodb_thread_concurrency=0 --innodb-buffer-pool-size=${par}GB --innodb-log-file-size=$logsz --innodb_flush_log_at_trx_commit=$trxv  &
/usr/local/mysql/libexec/mysqld --defaults-file=$CONFIG --datadir=$DR --innodb_data_home_dir=$log2 --innodb_log_group_home_dir=$log2 --innodb_thread_concurrency=0 --innodb-buffer-pool-size=${par}GB --innodb-log-file-size=$logsz --innodb_flush_log_at_trx_commit=$trxv  &

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




iostat -dmx 10 2000 >> $OUTDIR/iostat.${runid}res &
PID=$!
vmstat 10 2000 >> $OUTDIR/vmstat.${runid}res &
PIDV=$!
./virident_stat.sh >> $OUTDIR/virident.${runid}res &
PIDVS=$!
$MYSQLDIR/bin/mysqladmin ext -i10 -r >> $OUTDIR/mysqladminext.${runid}res &
PIDMYSQLSTAT=$!
./innodb_stat.sh >> $OUTDIR/innodb.${runid}res &
PIDINN=$!


cp $CONFIG $OUTDIR
cp $0 $OUTDIR
mysqladmin variables >>  $OUTDIR/mysql_variables.res
./tpcc_start localhost tpcc1000 root "" 1000 32 10 10800 | tee -a $OUTDIR/tpcc.${runid}.out
kill $PIDMYSQLSTAT
kill -9 $PID
kill -9 $PIDV
kill -9 $PIDVS
kill -9 $MYSQLPID
kill -9 $PIDINN

#waitm


#mysqladmin  shutdown


done

done
done
