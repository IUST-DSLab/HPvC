#include "monitor.h"

void *listen_update_time(void *params)
{
  zsock_t *sock = zsock_new_req("tcp://127.0.0.1:9888");
  while(1) {
    // get REFRESH_MS
  }
  pthread_exit(NULL);
}


int main(int argc, char *argv[]) {

  // create thread for update time
  // Use IGuest.getFacilityStatus for repors
  return 0;
}