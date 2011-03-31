cat $1 | grep LSN_ratio_expected | awk ' BEGIN {s=0} { print s,$4; s++ } ' > /tmp/1.rr
cat $1 | grep -P "LSN_ratio[\s]" | awk ' BEGIN {s=0} { print s,$4; s++ } ' > /tmp/2.rr
cat $1 | grep Innodb_log_write_sleeps | awk ' BEGIN {s=0;i=0} { print i,$4-s; s=$4;i++ } ' > /tmp/3.rr
cat $1 | grep -P "Innodb_log_write_sleep_prob" | awk ' BEGIN {s=0} { print s,$4; s++ } ' > /tmp/4.rr
cat $1 | grep -P "Innodb_break_ratio" | awk ' BEGIN {s=0} { print s,$4; s++ } ' > /tmp/5.rr
cat $1 | grep -P "Innodb_EMA_lsn_grow" | awk ' BEGIN {s=0} { print s,$4; s++ } ' > /tmp/6.rr
cat $1 | grep -P "Innodb_EMA_ckpt_lsn_grow" | awk ' BEGIN {s=0} { print s,$4; s++ } ' > /tmp/7.rr
cat $1 | grep -P "Innodb_buffer_pool_pages_flushed[\s]" |  awk ' BEGIN {s=0;i=0} { print i,$4-s; s=$4;i++ } ' > /tmp/8.rr
cat $2 | awk ' BEGIN {i=0;sp=0 } /Last checkpoint at/  { st=$4 ; print i, (st)/1024/1024,(st-sp)/1024/1024; i++; sp=st } ' > /tmp/9.rr

join /tmp/1.rr /tmp/2.rr > /tmp/12.rr
join /tmp/12.rr /tmp/3.rr > /tmp/13.rr
join /tmp/13.rr /tmp/4.rr > /tmp/14.rr
join /tmp/14.rr /tmp/5.rr > /tmp/15.rr
join /tmp/15.rr /tmp/6.rr > /tmp/16.rr
join /tmp/16.rr /tmp/7.rr > /tmp/17.rr
join /tmp/17.rr /tmp/8.rr > /tmp/18.rr
join /tmp/18.rr /tmp/9.rr > /tmp/19.rr
echo "time LSN_ratio_expected LSN_ratio sleeps sleep_prob break_ratio EMA_lsn EMA_ckpt_lsn  flushed checkpoint checkpoint_delta"
cat /tmp/19.rr
 
