#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    unsigned int playerCount;
    unsigned int gameCount;
    unsigned int startGame;
    unsigned int threadId;
    unsigned int threadCount;
    unsigned int *playerDrinks;
    uint32_t rngState;
} ThreadData;

static inline uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static inline uint32_t mix_seed(uint32_t x) {
  x += 0x9E3779B9u;
  x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
  x = (x ^ (x >> 13)) * 0xC2B2AE35u;
  x ^= x >> 16;

  return x ? x : 1u;
}

static void *simulate_games(void *arg) {
  ThreadData *data = (ThreadData *)arg;

  const unsigned int playerCount = data -> playerCount;
  const unsigned int gameCount = data -> gameCount;
  unsigned int *drinks = data -> playerDrinks;

  uint32_t rngState = data -> rngState;

  for (unsigned int game = data->threadId; game < gameCount; game += data->threadCount) {
    unsigned int *currentGameDrinks = drinks + (size_t)game * playerCount;
    
    unsigned int index = 0;
    unsigned char state = 0;
    bool isFinalStretch = false;

    while (1) {
      uint32_t r = xorshift32(&rngState);

      unsigned char roll = (unsigned char)(((int64_t)r * 6ULL) >> 32);
      unsigned char mask = (unsigned char)(1u << roll);

      bool hit = (state & mask) != 0;

      state ^= mask;

      currentGameDrinks[index] += (unsigned int) hit;

      if (!hit && ++index == playerCount) {
        index = 0;
      }

      isFinalStretch &= hit;
      isFinalStretch |= state == 0b111111;

      if (isFinalStretch && state == 0) {
        break;
      }
    }
  }

  data -> rngState = rngState;
  return NULL;
}

int main(int argc, char* argv[]) {
  unsigned int playerCount = (argc > 1) ? atoi(argv[1]) : 4;
  unsigned int gameCount = (argc > 2) ? atoi(argv[2]) : 1;
  unsigned int threadCount = (argc > 3) ? atoi(argv[3]) : 0;
  unsigned int debug = (argc > 4) ? atoi(argv[4]) : 0;

  if (playerCount < 1) playerCount = 1;
  if (gameCount < 1) gameCount = 1;

  if (threadCount < 1) {
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    threadCount = (cpus > 0) ? (unsigned int)cpus : 1;
    printf("Set thread count to: %d\n", threadCount);
  }

  if (threadCount > gameCount) {
    threadCount = gameCount;
    printf("More threads than games! Set to game count");
  }

  if ((size_t)gameCount > SIZE_MAX / (size_t)playerCount || (size_t)gameCount * playerCount > SIZE_MAX / sizeof(unsigned int)) {
    fprintf(stderr, "Requested simulation is too large.\n");
      return 1;
  }

  size_t total = (size_t)gameCount * playerCount;

  unsigned int *playerDrinks = calloc(total, sizeof(*playerDrinks));

  if (!playerDrinks) {
    fprintf(stderr, "Memory allocation failed!\n");
    free(playerDrinks);
    return 1;
  }

  pthread_t *threads = malloc((size_t)threadCount * sizeof(*threads));
  ThreadData *threadData = malloc((size_t)threadCount * sizeof(*threadData));

  if (!threads || !threadData) {
    fprintf(stderr, "Thread allocation failed!\n");
    free(threadData);
    free(threads);
    free(playerDrinks);
    return 1;
  }

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  uint32_t baseSeed = (uint32_t)time(NULL) ^ (uint32_t)start.tv_nsec ^ (uint32_t)getpid();

  unsigned int created = 0;
  
  for (unsigned int t = 0; t < threadCount; ++t) {
    threadData[t] = (ThreadData) {
      .playerCount = playerCount,
      .gameCount = gameCount,
      .startGame = t,
      .threadId = t,
      .threadCount = threadCount,
      .playerDrinks = playerDrinks,
      .rngState = mix_seed(baseSeed + t * 0x9E3779B9u)
    };

    int err = pthread_create(&threads[t], NULL, simulate_games, &threadData[t]);

    if (err != 0) {
      fprintf(stderr, "pthread_create failed: %d\n", err);
      break;
    }

    ++created;
  }

  for (unsigned int t = 0; t < created; ++t) {
    pthread_join(threads[t], NULL);
  }

  if (created != threadCount) {
    free(threadData);
    free(threads);
    free(playerDrinks);
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC, &end);

  double elapsedMs = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

  printf("Threads: %u\n", threadCount);
  printf("Time Taken: %.3f ms\n", elapsedMs);

  if (debug > 0) {
    printf("\nDrinks:\n");

    for (unsigned int i = 0; i < playerCount; ++i) {
      printf("\nPlayer %u: ", i + 1);

      for (unsigned int j = 0; j < gameCount; ++j) {
        printf("%u ", playerDrinks[(size_t)j * playerCount + i]);
      }
    }

    printf("\n");
  }

  free(threadData);
  free(threads);
  free(playerDrinks);

  return 0;
}