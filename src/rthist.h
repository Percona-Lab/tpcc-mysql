/*
 * rthist.h
 */

void hist_init();
void hist_inc( int transaction, time_t rtclk );
float hist_ckp( int transaction );
void hist_report();
