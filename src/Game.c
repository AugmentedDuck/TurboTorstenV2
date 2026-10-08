#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

static inline uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

int main(int argc, char* argv[]) {
  unsigned int playerCount = (argc > 1) ? atoi(argv[1]) : 4;
  unsigned int gameCount = (argc > 2) ? atoi(argv[2]) : 1;

  if (playerCount < 1) playerCount = 1;
  if (gameCount < 1) gameCount = 1;

  //unsigned int *playerDrinks = calloc((size_t)gameCount * playerCount, sizeof(unsigned int));
  unsigned int playerDrinks[gameCount][playerCount]; 

  for (int i = 0; i < playerCount; i++) {
    for (int j = 0; j < gameCount; j++) {
      playerDrinks[j][i] = 0;
    }
  }

  if (!playerDrinks) {
    fprintf(stderr, "Memory allocation failed!\n");
    return 1;
  }

  unsigned char state = 0;
  unsigned int index = 0;
  unsigned int game = 0;
  bool isFinalStretch = false;

  uint32_t rngState = (uint32_t)time(NULL) | 1;

  //unsigned int *currentGameDrinks = playerDrinks;

  srand((unsigned)time(NULL));

  clock_t start = clock(), diff;

  while (1) {
    uint32_t r = xorshift32(&rngState);
    unsigned char roll = (unsigned char)(((uint64_t)r * 6ULL) >> 32);
    
    unsigned char mask = 1u << roll;
    bool hit = state & mask;
    
    state ^= mask;

    playerDrinks[game][index] += hit;
    
    if (!hit) {
      if (++index == playerCount) {
        index = 0;
      }
    }

    isFinalStretch &= hit;

    isFinalStretch |= state == 0b111111;
    

    if (isFinalStretch && state == 0) {
      if (++game == gameCount) {
        break;
      }
    }
  }

  diff = clock() - start;

  int msec = diff * 1000 / CLOCKS_PER_SEC;

  printf("Time Taken: %d ms\n", msec);

  /*
  printf("\nDrinks:\n");

  for (int i = 0; i < playerCount; i++) {
    printf("\nPlayer %d: ", i + 1);
    for (int j = 0; j < gameCount; j++) {
      printf("%u ", playerDrinks[j][i]);
    }
  }
  printf("\n");
  */

  return 0;
}