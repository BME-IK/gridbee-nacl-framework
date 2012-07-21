#include <cstdio>
#include <cstdlib>
#include "../gridbee_boinc_framework/boinc_api.h"

int my_main(){

  boinc_init();
  
  for(double percent=0; percent<100; percent+=10){
    boinc_fraction_done(percent/100);
    if(boinc_time_to_checkpoint())
      boinc_checkpoint_completed();
    sleep(2);
  }
  boinc_fraction_done(1);    
  boinc_finish(0);  
  return 0;
}
