#include "contiki.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include "lib/random.h"
 
PROCESS(listen_gateway, "Listening messages from the gateway");
PROCESS(listen_sensors, "Listening messages from sensors");
AUTOSTART_PROCESSES(&listen_gateway,&listen_sensors);
 
 PROCESS_THREAD(listen_gateway, ev, data)
 {
   PROCESS_BEGIN();
 
   for(;;) {
     PROCESS_YIELD();
     if(ev == serial_line_event_message) {
		
       printf("received line: %s \n", (char *)data);
     }
   }
   PROCESS_END();
 }
 
PROCESS_THREAD(listen_sensors, ev, data)
{
  static struct etimer et;
  const char *type[2];
  type[0] = "temperature";
  type[1] = "humidity";
  PROCESS_BEGIN();
  /* set timer to expire after 5 seconds */
  etimer_set(&et, CLOCK_SECOND * 10);
  while(1) {
    PROCESS_WAIT_EVENT(); /* Same thing as PROCESS_YIELD */
    if(etimer_expired(&et)) {
      /* Do the work here and restart timer to get it periodic !!! */
      printf("# %d %d %d\n", random_rand()%3, (random_rand()%2)+1 , random_rand()%100); 
      etimer_restart(&et);
    }
  }
  PROCESS_END();
}
