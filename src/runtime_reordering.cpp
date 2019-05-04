#include <semaphore.h>

#include <random>
#include <thread>
#include <iostream>

// compilation options
// g++ -std=c++11 -lpthread

#define USE_CPU_FENCE 0

sem_t begin_sema1;
sem_t begin_sema2;
sem_t end_sema;

int X, Y;
int r1, r2;

void thread1_func() {
  std::mt19937 rng(std::random_device("/dev/random")());
  std::uniform_int_distribution<int> random(1, 100000000);
  for (;;) {
    sem_wait(&begin_sema1);  // Wait for signal
    // Random delay is necessary to increase the posibility of capaturing CPU
    // reordering
    while (random(rng) % 8 != 0) {}

    X = 1;
#if USE_CPU_FENCE
    asm volatile("mfence" ::: "memory");  // Prevent CPU reordering
#else
    asm volatile("" ::: "memory");  // Prevent compiler reordering
#endif
    r1 = Y;

    sem_post(&end_sema);  // Notify transaction complete
  }
};

void thread2_func() {
  std::mt19937 rng(std::random_device("/dev/random")() + 10);
  std::uniform_int_distribution<int> random(1, 100000000);
  for (;;) {
    sem_wait(&begin_sema2);  // Wait for signal
    // Random delay is necessary to increase the posibility of capaturing CPU
    // reordering
    while (random(rng) % 8 != 0) {}  // Random delay

    Y = 1;
#if USE_CPU_FENCE
    asm volatile("mfence" ::: "memory");  // Prevent CPU reordering
#else
    asm volatile("" ::: "memory");  // Prevent compiler reordering
#endif
    r2 = X;

    sem_post(&end_sema);  // Notify transaction complete
  }
};

int main() {
  sem_init(&begin_sema1, 0, 0);
  sem_init(&begin_sema2, 0, 0);
  sem_init(&end_sema, 0, 0);

  std::thread thread1([] { thread1_func(); });
  std::thread thread2([] { thread2_func(); });

  // Repeat the experiment ad infinitum
  int detected = 0;
  for (int iterations = 1; ; ++iterations) {
    // Reset X and Y
    X = 0;
    Y = 0;
    // Signal both threads
    sem_post(&begin_sema1);
    sem_post(&begin_sema2);
    // Wait for both threads
    sem_wait(&end_sema);
    sem_wait(&end_sema);
    // Check if there was a simultaneous reorder
    if (r1 == 0 && r2 == 0) {
      std::cout << "gotcha! "
        << ++detected << " reorders detected after " << iterations
        << " iterations" << std::endl;
    }
  }
  return 0;  // Never returns
}
// vim: et tw=80 ts=2 sw=2 cc=80:
