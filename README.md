1. Build binaries
   * `cd src ; make`
   ( you should have mysql_config available in $PATH)

2. Load data
   * create database
     `mysqladmin create tpcc1000`
   * create tables
     `mysql tpcc1000 < create_table.sql`
   * create indexes and FK ( this step can be done after loading data)
     `mysql tpcc1000 < add_fkey_idx.sql`
   * populate data
     - simple step
       `tpcc_load -h127.0.0.1 -d tpcc1000 -u root -p "" -w 1000`
                 |hostname:port| |dbname| |user| |password| |WAREHOUSES|
       ref. tpcc_load --help for all options
     - load data in parallel 
       check load.sh script

3. Start benchmark
   * `./tpcc_start -h127.0.0.1 -P3306 -dtpcc1000 -uroot -w1000 -c32 -r10 -l10800`
   * |hostname| |port| |dbname| |user| |WAREHOUSES| |CONNECTIONS| |WARMUP TIME| |BENCHMARK TIME|
   * ref. tpcc_start --help for all options 
