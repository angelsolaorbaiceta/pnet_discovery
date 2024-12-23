#include "broadcast.h"
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>

int main() {
  pthread_t broadcast_thread, sender_thread, response_thread;

  init_my_info();
  printf("Hello %s! Your token is %s\n", my_username, my_token);

  pthread_create(&broadcast_thread, NULL, handle_broadcast, NULL);
  pthread_create(&sender_thread, NULL, send_broadcast, NULL);
  pthread_create(&response_thread, NULL, handle_responses, NULL);

  while (1) {
    pthread_mutex_lock(&peers_mutex);
    if (peer_count > 0) {
      printf("\nCurrent peers (%d):\n", peer_count);
      Peer *peer = peers;
      while (peer != NULL) {
        printf("\t%s at %s (last seen: %ld seconds ago)\n", peer->username,
               peer->ip, time(NULL) - peer->last_seen);

        peer = peer->next;
      }
    } else {
      printf("\nNo peers discovered yet.");
    }

    pthread_mutex_unlock(&peers_mutex);
    sleep(5);
  }

  return 0;
}
