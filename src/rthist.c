/*
 * rthist.c
 * RT-histgram
 */

#include <stdio.h>
#include <sys/time.h>

#define MAXREC      320
#define REC_PER_SEC 5

extern float max_rt[];
extern long clk_tck;

int total_hist[5][MAXREC * REC_PER_SEC];
int cur_hist[5][MAXREC * REC_PER_SEC];

/* initialize */
void hist_init()
{
  int i,j;

  for( i=0; i<5; i++){
    for( j=0; j<(MAXREC * REC_PER_SEC); j++){
      total_hist[i][j] = cur_hist[i][j] = 0;
    }
  }
}

/* incliment matched one */
void hist_inc( int transaction, time_t rtclk )
{
  int i;

  i = ( rtclk * REC_PER_SEC )/clk_tck;
  if(i >= (MAXREC * REC_PER_SEC)){
    i = (MAXREC * REC_PER_SEC) - 1;
  }
  ++cur_hist[transaction][i];
}

/* check point, add on total histgram, return 90% line */
float hist_ckp( int transaction )
{
  int i;
  int total,tmp,line;

  total = tmp = 0;
  line = MAXREC * REC_PER_SEC;
  for( i=0; i<(MAXREC * REC_PER_SEC); i++){
    total += cur_hist[transaction][i];
  }
  for( i=(MAXREC * REC_PER_SEC)-1; i >= 0 ; i--){
    tmp += cur_hist[transaction][i];
    total_hist[transaction][i] += cur_hist[transaction][i];
    cur_hist[transaction][i] = 0;
    if( (tmp * 10) <= total ){
      line = i;
    }
  }
  return ( (float)(line)/(float)(REC_PER_SEC) );
}

void hist_report()
{
  int i,j;
  int total[5],tmp[5],line[5];

  for( j=0; j<5; j++){
    total[j] = tmp[j] = 0;
    line[j] = MAXREC * REC_PER_SEC;
    for( i=0; i<(MAXREC * REC_PER_SEC); i++){
      total[j] += total_hist[j][i];
    }
    for( i=(MAXREC * REC_PER_SEC)-1; i >= 0 ; i--){
      tmp[j] += total_hist[j][i];
      if( (tmp[j] * 10) <= total[j] ){
	line[j] = i;
      }
    }
  }

  printf("\n<RT Histogram>\n");

  for( j=0; j<5; j++){
    switch(j){
    case 0:
      printf("\n1.New-Order\n\n");
      break;
    case 1:
      printf("\n2.Payment\n\n");
      break;
    case 2:
      printf("\n3.Order-Status\n\n");
      break;
    case 3:
      printf("\n4.Delivery\n\n");
      break;
    case 4:
      printf("\n5.Stock-Level\n\n");
    }
    for( i=0; (i<(MAXREC * REC_PER_SEC))&&(i <= line[j]*4); i++){
      printf("%3.2f, %6d\n",(float)(i+1)/(float)(REC_PER_SEC),total_hist[j][i]);
    }
    printf("\n");
  }

  printf("\n<90th Percentile RT (MaxRT)>\n");
  for( j=0; j<5; j++){
    switch(j){
    case 0:
      printf("   New-Order : ");
      break;
    case 1:
      printf("     Payment : ");
      break;
    case 2:
      printf("Order-Status : ");
      break;
    case 3:
      printf("    Delivery : ");
      break;
    case 4:
      printf(" Stock-Level : ");
    }
    printf("%3.2f  (%.2f)\n",(float)(line[j])/(float)(REC_PER_SEC),max_rt[j]);
  }

}




