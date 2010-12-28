while [ true ]
do

mysql -e "SHOW ENGINE INNODB STATUS\G" 
sleep 10

done

