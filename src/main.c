/*
 * main.pc 
 * driver for the tpcc transactions
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>

#include <mysql.h>

#include "tpc.h"
#include "trans_if.h"
#include "spt_proc.h"
#include "sequence.h"
#include "rthist.h"

/* Global SQL Variables */
MYSQL **ctx;
MYSQL_STMT ***stmt;

#define DB_STRING_MAX 51

char connect_string[DB_STRING_MAX];
char db_string[DB_STRING_MAX];
char db_user[DB_STRING_MAX];
char db_password[DB_STRING_MAX];

int num_ware;
int num_conn;
int lampup_time;
int measure_time;

int num_node; /* number of servers that consists of cluster i.e. RAC (0:normal mode)*/
#define NUM_NODE_MAX 8
char node_string[NUM_NODE_MAX][DB_STRING_MAX];

int time_count;
#define PRINT_INTERVAL 10

int success[5];
int late[5];
int retry[5];
int failure[5];

int* success2[5];
int* late2[5];
int* retry2[5];
int* failure2[5];

int success2_sum[5];
int late2_sum[5];
int retry2_sum[5];
int failure2_sum[5];

int prev_s[5];
int prev_l[5];

float max_rt[5];

int activate_transaction;
int counting_on;

long clk_tck;

int is_local = 0; /* "1" mean local */
int valuable_flg = 0; /* "1" mean valuable ratio */


typedef struct
{
  int number;
  int port;
} thread_arg;
int thread_main(thread_arg* arg);

void alarm_handler(int signum);
void alarm_dummy();

inline void parse_host(char* host, const char* from)
{
  const char* end= index(from,':');
  size_t length;
  if(end == NULL)
    length= strlen(from);
  else
    length= (end - from);
  memcpy(host,from, length);
  host[length] = '\0';
}
inline int parse_port(const char* from)
{
  const char* end= index(from,':');
  if(end == NULL)
    return 3306;
  else
    return atoi(end);
}
int main( int argc, char *argv[] )
{
  int i, k, t_num, arg_offset;
  long j;
  float f;
  pthread_t *t;
  timer_t timer;
  struct itimerval itval;
  struct sigaction  sigact;
  int port= 3306;
  int fd, seed;

  printf("***************************************\n");
  printf("*** ###easy### TPC-C Load Generator ***\n");
  printf("***************************************\n");

  /* initialize */
  hist_init();
  activate_transaction = 1;
  counting_on = 0;

  for ( i=0; i<5; i++ ){
    success[i]=0;
    late[i]=0;
    retry[i]=0;
    failure[i]=0;

    prev_s[i]=0;
    prev_l[i]=0;

    max_rt[i]=0.0;
  }

  /* dummy initialize*/
  num_ware = 1;
  num_conn = 10;
  lampup_time = 10;
  measure_time = 20;
  strcpy( connect_string, "tpcc/tpcc" );
  strcpy( db_string, "tpcc" );

  /* number of node (default 0) */
  num_node = 0;
  arg_offset = 0;

  /* alarm initialize */
  time_count = 0;
  itval.it_interval.tv_sec = PRINT_INTERVAL;
  itval.it_interval.tv_usec = 0;
  itval.it_value.tv_sec = PRINT_INTERVAL;
  itval.it_value.tv_usec = 0;
  sigact.sa_handler = alarm_handler;
  sigact.sa_flags = 0;
  sigemptyset(&sigact.sa_mask);

  /* setup handler&timer */
  if( sigaction( SIGALRM, &sigact, NULL ) == -1 ) {
    fprintf(stderr, "error in sigaction()\n");
    exit(1);
  }

  clk_tck = sysconf(_SC_CLK_TCK);

  /* Parse args */

  if ((num_node == 0)&&(argc == 14)) { /* hidden mode */
    valuable_flg = 1;
  }
      
  if ((num_node == 0)&&(valuable_flg == 0)&&(argc != 9)) {
    fprintf(stderr, "\n usage: tpcc_start [server] [DB] [user] [pass] [warehouse] [connection] [rampup] [measure]\n");
    exit(1);
  }

  if ( strlen(argv[1]) >= DB_STRING_MAX ) {
    fprintf(stderr, "\n server phrase is too long\n");
    exit(1);
  }
  if ( strlen(argv[2]) >= DB_STRING_MAX ) {
    fprintf(stderr, "\n DBname phrase is too long\n");
    exit(1);
  }
  if ( strlen(argv[3]) >= DB_STRING_MAX ) {
    fprintf(stderr, "\n user phrase is too long\n");
    exit(1);
  }
  if ( strlen(argv[4]) >= DB_STRING_MAX ) {
    fprintf(stderr, "\n pass phrase is too long\n");
    exit(1);
  }
  if ((num_ware = atoi(argv[5 + arg_offset])) <= 0) {
    fprintf(stderr, "\n expecting positive number of warehouses\n");
    exit(1);
  }
  if ((num_conn = atoi(argv[6 + arg_offset])) <= 0) {
    fprintf(stderr, "\n expecting positive number of connections\n");
    exit(1);
  }
  if ((lampup_time = atoi(argv[7 + arg_offset])) < 0) {
    fprintf(stderr, "\n expecting positive number of lampup_time [sec]\n");
    exit(1);
  }
  if ((measure_time = atoi(argv[8 + arg_offset])) < 0) {
    fprintf(stderr, "\n expecting positive number of measure_time [sec]\n");
    exit(1);
  }
  parse_host(connect_string, argv[1]);
  port= parse_port(argv[1]);
  strcpy( db_string, argv[2] );
  strcpy( db_user, argv[3] );
  strcpy( db_password, argv[4] );
  if(strcmp(db_string,"l")==0){
    is_local = 1;
  }else{
    is_local = 0;
  }

  if(valuable_flg==1){
    if( (atoi(argv[9 + arg_offset]) < 0)||(atoi(argv[10 + arg_offset]) < 0)||(atoi(argv[11 + arg_offset]) < 0)
	||(atoi(argv[12 + arg_offset]) < 0)||(atoi(argv[13 + arg_offset]) < 0) ) {
      fprintf(stderr, "\n expecting positive number of ratio parameters\n");
      exit(1);
    }
  }

  if( num_node > 0 ){
    if( num_ware % num_node != 0 ){
      fprintf(stderr, "\n [warehouse] value must be devided by [num_node].\n");
      exit(1);
    }
    if( num_conn % num_node != 0 ){
      fprintf(stderr, "\n [connection] value must be devided by [num_node].\n");
      exit(1);
    }
  }

  printf("<Parameters>\n");
  if(is_local==0)printf("     [server]: %s\n", connect_string);
  printf("     [DBname]: %s\n", db_string);
  printf("       [user]: %s\n", db_user);
  printf("       [pass]: %s\n", db_password);

  printf("  [warehouse]: %d\n", num_ware);
  printf(" [connection]: %d\n", num_conn);
  printf("     [rampup]: %d (sec.)\n", lampup_time);
  printf("    [measure]: %d (sec.)\n", measure_time);

  if(valuable_flg==1){
    printf("      [ratio]: %d:%d:%d:%d:%d\n", atoi(argv[9 + arg_offset]), atoi(argv[10 + arg_offset]),
	   atoi(argv[11 + arg_offset]), atoi(argv[12 + arg_offset]), atoi(argv[13 + arg_offset]) );
  }

  fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1) {
    fd = open("/dev/random", O_RDONLY);
    if (fd == -1) {
      struct timeval  tv;
      gettimeofday(&tv, NULL);
      seed = (tv.tv_sec ^ tv.tv_usec) * tv.tv_sec * tv.tv_usec ^ tv.tv_sec;
    }else{
      read(fd, &seed, sizeof(seed));
      close(fd);
    }
  }else{
    read(fd, &seed, sizeof(seed));
    close(fd);
  }
  SetSeed(seed);

  if(valuable_flg==0){
    seq_init(10,10,1,1,1); /* normal ratio */
  }else{
    seq_init( atoi(argv[9 + arg_offset]), atoi(argv[10 + arg_offset]), atoi(argv[11 + arg_offset]),
	      atoi(argv[12 + arg_offset]), atoi(argv[13 + arg_offset]) );
  }

  /* set up each counter */
  for ( i=0; i<5; i++ ){
      success2[i] = malloc( sizeof(int) * num_conn );
      late2[i] = malloc( sizeof(int) * num_conn );
      retry2[i] = malloc( sizeof(int) * num_conn );
      failure2[i] = malloc( sizeof(int) * num_conn );
      for ( k=0; k<num_conn; k++ ){
	  success2[i][k] = 0;
	  late2[i][k] = 0;
	  retry2[i][k] = 0;
	  failure2[i][k] = 0;
      }
  }

  /* set up threads */

  t = malloc( sizeof(pthread_t) * num_conn );
  if ( t == NULL ){
    fprintf(stderr, "error at malloc(pthread_t)\n");
    exit(1);
  }

  ctx = malloc( sizeof(MYSQL *) * num_conn );
  stmt = malloc( sizeof(MYSQL_STMT **) * num_conn );
  for( i=0; i < num_conn; i++ ){
      stmt[i] = malloc( sizeof(MYSQL_STMT *) * 40 );
  }

  if ( ctx == NULL ){
    fprintf(stderr, "error at malloc(sql_context)\n");
    exit(1);
  }

  /* EXEC SQL WHENEVER SQLERROR GOTO sqlerr; */

  for( i=0; i < num_conn; i++ ){
    ctx[i] = mysql_init(NULL);
    if(!ctx[i]) goto sqlerr;
  }

  for( t_num=0; t_num < num_conn; t_num++ ){
    thread_arg arg;
    arg.number= t_num;
    arg.port= port;
    pthread_create( &t[t_num], NULL, (void *)thread_main, (void *)&arg );
  }


  printf("\nRAMP-UP TIME.(%d sec.)\n",lampup_time);
  fflush(stdout);
  sleep(lampup_time);
  printf("\nMEASURING START.\n\n");
  fflush(stdout);

  /* sleep(measure_time); */
  /* start timer */

#ifndef _SLEEP_ONLY_
  if( setitimer(ITIMER_REAL, &itval, NULL) == -1 ) {
    fprintf(stderr, "error in setitimer()\n");
  }
#endif

  counting_on = 1;
  /* wait signal */
  for(i = 0; i < (measure_time / PRINT_INTERVAL); i++ ) {
#ifndef _SLEEP_ONLY_
    pause();
#else
    sleep(PRINT_INTERVAL);
    alarm_dummy();
#endif
  }
  counting_on = 0;

#ifndef _SLEEP_ONLY_
  /* stop timer */
  itval.it_interval.tv_sec = 0;
  itval.it_interval.tv_usec = 0;
  itval.it_value.tv_sec = 0;
  itval.it_value.tv_usec = 0;
  if( setitimer(ITIMER_REAL, &itval, NULL) == -1 ) {
    fprintf(stderr, "error in setitimer()\n");
  }
#endif

  printf("\nSTOPPING THREADS");
  activate_transaction = 0;

  /* wait threads' ending and close connections*/
  for( i=0; i < num_conn; i++ ){
    pthread_join( t[i], NULL );
  }

  printf("\n");

  free(ctx);
  for( i=0; i < num_conn; i++ ){
      free(stmt[i]);
  }
  free(stmt);

  free(t);

  hist_report();

  printf("\n<Raw Results>\n");
  for ( i=0; i<5; i++ ){
    printf("  [%d] sc:%d  lt:%d  rt:%d  fl:%d \n", i, success[i], late[i], retry[i], failure[i]);
  }
  printf(" in %d sec.\n", (measure_time / PRINT_INTERVAL) * PRINT_INTERVAL);

  printf("\n<Raw Results2(sum ver.)>\n");
  for( i=0; i<5; i++ ){
      success2_sum[i] = 0;
      late2_sum[i] = 0;
      retry2_sum[i] = 0;
      failure2_sum[i] = 0;
      for( k=0; k<num_conn; k++ ){
	  success2_sum[i] += success2[i][k];
	  late2_sum[i] += late2[i][k];
	  retry2_sum[i] += retry2[i][k];
	  failure2_sum[i] += failure2[i][k];
      }
  }
  for ( i=0; i<5; i++ ){
      printf("  [%d] sc:%d  lt:%d  rt:%d  fl:%d \n", i, success2_sum[i], late2_sum[i], retry2_sum[i], failure2_sum[i]);
  }

  printf("\n<Constraint Check> (all must be [OK])\n [transaction percentage]\n");
  for ( i=0, j=0; i<5; i++ ){
    j += (success[i] + late[i]);
  }

  f = 100.0 * (float)(success[1] + late[1])/(float)j;
  printf("        Payment: %3.2f%% (>=43.0%%)",f);
  if ( f >= 43.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }
  f = 100.0 * (float)(success[2] + late[2])/(float)j;
  printf("   Order-Status: %3.2f%% (>= 4.0%%)",f);
  if ( f >= 4.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }
  f = 100.0 * (float)(success[3] + late[3])/(float)j;
  printf("       Delivery: %3.2f%% (>= 4.0%%)",f);
  if ( f >= 4.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }
  f = 100.0 * (float)(success[4] + late[4])/(float)j;
  printf("    Stock-Level: %3.2f%% (>= 4.0%%)",f);
  if ( f >= 4.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }

  printf(" [response time (at least 90%% passed)]\n");
  f = 100.0 * (float)success[0]/(float)(success[0] + late[0]);
  printf("      New-Order: %3.2f%% ",f);
  if ( f >= 90.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }
  f = 100.0 * (float)success[1]/(float)(success[1] + late[1]);
  printf("        Payment: %3.2f%% ",f);
  if ( f >= 90.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }
  f = 100.0 * (float)success[2]/(float)(success[2] + late[2]);
  printf("   Order-Status: %3.2f%% ",f);
  if ( f >= 90.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }
  f = 100.0 * (float)success[3]/(float)(success[3] + late[3]);
  printf("       Delivery: %3.2f%% ",f);
  if ( f >= 90.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }
  f = 100.0 * (float)success[4]/(float)(success[4] + late[4]);
  printf("    Stock-Level: %3.2f%% ",f);
  if ( f >= 90.0 ){
    printf(" [OK]\n");
  }else{
    printf(" [NG] *\n");
  }

  printf("\n<TpmC>\n");
  f = (float)(success[0] + late[0]) * 60.0 
    / (float)((measure_time / PRINT_INTERVAL) * PRINT_INTERVAL);
  printf("                 %.3f TpmC\n",f);
  exit(0);

 sqlerr:
  fprintf(stdout, "error at main\n");
  error(ctx[i],0);
  exit(1);

}


void alarm_handler(int signum)
{
  int i;
  int s[5],l[5];
  float rt90[5];

  for( i=0; i<5; i++ ){
    s[i] = success[i];
    l[i] = late[i];
    rt90[i] = hist_ckp(i);
  }

  time_count += PRINT_INTERVAL;
  printf("%4d, %d(%d):%.1f, %d(%d):%.1f, %d(%d):%.1f, %d(%d):%.1f, %d(%d):%.1f\n",
	 time_count,
	 ( s[0] + l[0] - prev_s[0] - prev_l[0] ),
	 ( l[0] - prev_l[0] ),
	 rt90[0],
	 ( s[1] + l[1] - prev_s[1] - prev_l[1] ),
	 ( l[1] - prev_l[1] ),
	 rt90[1],
	 ( s[2] + l[2] - prev_s[2] - prev_l[2] ),
	 ( l[2] - prev_l[2] ),
	 rt90[2],
	 ( s[3] + l[3] - prev_s[3] - prev_l[3] ),
	 ( l[3] - prev_l[3] ),
	 rt90[3],
	 ( s[4] + l[4] - prev_s[4] - prev_l[4] ),
	 ( l[4] - prev_l[4] ),
	 rt90[4]
	 );
  fflush(stdout);

  for( i=0; i<5; i++ ){
    prev_s[i] = s[i];
    prev_l[i] = l[i];
  }
}

void alarm_dummy()
{
  int i;
  int s[5],l[5];
  float rt90[5];

  for( i=0; i<5; i++ ){
    s[i] = success[i];
    l[i] = late[i];
    rt90[i] = hist_ckp(i);
  }

  time_count += PRINT_INTERVAL;
  printf("%4d, %d(%d):%.1f, %d(%d):%.1f, %d(%d):%.1f, %d(%d):%.1f, %d(%d):%.1f\n",
	 time_count,
	 ( s[0] + l[0] - prev_s[0] - prev_l[0] ),
	 ( l[0] - prev_l[0] ),
	 rt90[0],
	 ( s[1] + l[1] - prev_s[1] - prev_l[1] ),
	 ( l[1] - prev_l[1] ),
	 rt90[1],
	 ( s[2] + l[2] - prev_s[2] - prev_l[2] ),
	 ( l[2] - prev_l[2] ),
	 rt90[2],
	 ( s[3] + l[3] - prev_s[3] - prev_l[3] ),
	 ( l[3] - prev_l[3] ),
	 rt90[3],
	 ( s[4] + l[4] - prev_s[4] - prev_l[4] ),
	 ( l[4] - prev_l[4] ),
	 rt90[4]
	 );
  fflush(stdout);

  for( i=0; i<5; i++ ){
    prev_s[i] = s[i];
    prev_l[i] = l[i];
  }
}

int thread_main (thread_arg* arg)
{
  int t_num= arg->number;
  int port= arg->port;
  int r,i;

  char *db_string_ptr;
  MYSQL* resp;

  db_string_ptr = db_string;

  /* EXEC SQL WHENEVER SQLERROR GOTO sqlerr;*/

  if(num_node > 0){ /* RAC mode */
    db_string_ptr = node_string[((num_node * t_num)/num_conn)];
  }

  if(is_local==1){
    /* exec sql connect :connect_string; */
    resp = mysql_real_connect(ctx[t_num], "localhost", db_user, db_password, db_string, port, NULL, 0);
  }else{
    /* exec sql connect :connect_string USING :db_string; */
    resp = mysql_real_connect(ctx[t_num], connect_string, db_user, db_password, db_string, port, NULL, 0);
  }

  if(resp) {
    mysql_autocommit(ctx[t_num], 0);
  } else {
    mysql_close(ctx[t_num]);
    goto sqlerr;
  }

  for(i=0;i<40;i++){
      stmt[t_num][i] = mysql_stmt_init(ctx[t_num]);
      if(!stmt[t_num][i]) goto sqlerr;
  }

  /* Prepare ALL of SQLs */
  if( mysql_stmt_prepare(stmt[t_num][0], "SELECT c_discount, c_last, c_credit, w_tax FROM customer, warehouse WHERE w_id = ? AND c_w_id = w_id AND c_d_id = ? AND c_id = ?", 128) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][1], "SELECT d_next_o_id, d_tax FROM district WHERE d_id = ? AND d_w_id = ? FOR UPDATE", 80) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][2], "UPDATE district SET d_next_o_id = ? + 1 WHERE d_id = ? AND d_w_id = ?", 69) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][3], "INSERT INTO orders (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local) VALUES(?, ?, ?, ?, ?, ?, ?)", 111) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][4], "INSERT INTO new_orders (no_o_id, no_d_id, no_w_id) VALUES (?,?,?)", 65) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][5], "SELECT i_price, i_name, i_data FROM item WHERE i_id = ?", 55) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][6], "SELECT s_quantity, s_data, s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05, s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10 FROM stock WHERE s_i_id = ? AND s_w_id = ? FOR UPDATE", 189) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][7], "UPDATE stock SET s_quantity = ? WHERE s_i_id = ? AND s_w_id = ?", 63) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][8], "INSERT INTO order_line (ol_o_id, ol_d_id, ol_w_id, ol_number, ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_dist_info) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)", 159) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][9], "UPDATE warehouse SET w_ytd = w_ytd + ? WHERE w_id = ?", 53) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][10], "SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name FROM warehouse WHERE w_id = ?", 91) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][11], "UPDATE district SET d_ytd = d_ytd + ? WHERE d_w_id = ? AND d_id = ?", 67) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][12], "SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name FROM district WHERE d_w_id = ? AND d_id = ?", 105) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][13], "SELECT count(c_id) FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_last = ?", 79) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][14], "SELECT c_id FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_last = ? ORDER BY c_first", 89) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][15], "SELECT c_first, c_middle, c_last, c_street_1, c_street_2, c_city, c_state, c_zip, c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_id = ? FOR UPDATE", 215) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][16], "SELECT c_data FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_id = ?", 72) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][17], "UPDATE customer SET c_balance = ?, c_data = ? WHERE c_w_id = ? AND c_d_id = ? AND c_id = ?", 90) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][18], "UPDATE customer SET c_balance = ? WHERE c_w_id = ? AND c_d_id = ? AND c_id = ?", 78) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][19], "INSERT INTO history(h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data) VALUES(?, ?, ?, ?, ?, ?, ?, ?)", 120) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][20], "SELECT count(c_id) FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_last = ?", 79) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][21], "SELECT c_balance, c_first, c_middle, c_last FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_last = ? ORDER BY c_first", 121) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][22], "SELECT c_balance, c_first, c_middle, c_last FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_id = ?", 102) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][23], "SELECT o_id, o_entry_d, COALESCE(o_carrier_id,0) FROM orders WHERE o_w_id = ? AND o_d_id = ? AND o_c_id = ? AND o_id = (SELECT MAX(o_id) FROM orders WHERE o_w_id = ? AND o_d_id = ? AND o_c_id = ?)", 196) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][24], "SELECT ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d FROM order_line WHERE ol_w_id = ? AND ol_d_id = ? AND ol_o_id = ?", 135) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][25], "SELECT COALESCE(MIN(no_o_id),0) FROM new_orders WHERE no_d_id = ? AND no_w_id = ?", 81) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][26], "DELETE FROM new_orders WHERE no_o_id = ? AND no_d_id = ? AND no_w_id = ?", 72) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][27], "SELECT o_c_id FROM orders WHERE o_id = ? AND o_d_id = ? AND o_w_id = ?", 70) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][28], "UPDATE orders SET o_carrier_id = ? WHERE o_id = ? AND o_d_id = ? AND o_w_id = ?", 79) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][29], "UPDATE order_line SET ol_delivery_d = ? WHERE ol_o_id = ? AND ol_d_id = ? AND ol_w_id = ?", 89) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][30], "SELECT SUM(ol_amount) FROM order_line WHERE ol_o_id = ? AND ol_d_id = ? AND ol_w_id = ?", 87) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][31], "UPDATE customer SET c_balance = c_balance + ? , c_delivery_cnt = c_delivery_cnt + 1 WHERE c_id = ? AND c_d_id = ? AND c_w_id = ?", 128) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][32], "SELECT d_next_o_id FROM district WHERE d_id = ? AND d_w_id = ?", 62) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][33], "SELECT DISTINCT ol_i_id FROM order_line WHERE ol_w_id = ? AND ol_d_id = ? AND ol_o_id < ? AND ol_o_id >= (? - 20)", 113) ) goto sqlerr;
  if( mysql_stmt_prepare(stmt[t_num][34], "SELECT count(*) FROM stock WHERE s_w_id = ? AND s_i_id = ? AND s_quantity < ?", 77) ) goto sqlerr;

  r = driver(t_num);

  /* EXEC SQL COMMIT WORK; */
  if( mysql_commit(ctx[t_num]) ) goto sqlerr;

  for(i=0;i<40;i++){
      mysql_stmt_free_result(stmt[t_num][i]);
      mysql_stmt_close(stmt[t_num][i]);
  }

  /* EXEC SQL DISCONNECT; */
  mysql_close(ctx[t_num]);

  printf(".");
  fflush(stdout);

  return(r);

 sqlerr:
  fprintf(stdout, "error at thread_main\n");
  error(ctx[t_num],0);
  return(0);

}
