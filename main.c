#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Some GCC-specific macros which help indicate to the compiler whether some expressions are expected to give a certain
 * result or another. This will be accounted for during branch prediction.
 */
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

// UNION-FIND WITH PATH COMPRESSION

/**
 * An item from our union-find data structure.
 */
typedef struct uf_item {
  int parent;
  int rank;
} uf_item_t;

/**
 * Finds the representative of u in the union-find array.
 * @param items the union-find array.
 * @param u the item we're searching a representative for.
 * @return the found representative.
 */
int uf_find(uf_item_t *items, int u) {
  uf_item_t item = items[u];
  while (item.parent != u && items[item.parent].parent != item.parent) {
    item.parent = items[item.parent].parent;
  }
  return item.parent;
}

/**
 * Makes the union of two items in the union-find data structure.
 * @param items the union-find array.
 * @param u the first item we're making the UF for.
 * @param v the second item we're making the UF for.
 */
void uf_union_r(uf_item_t *items, int ur, int vr) {
  if (items[ur].rank < items[vr].rank) {
    items[ur].parent = vr;
  } else {
    items[vr].parent = ur;
    if (items[ur].rank == items[vr].rank) {
      items[ur].rank++;
    }
  }
}

/*
 * The masks we'll be using in order to implement radix sorting on the bridges. The cost is bound between 1 and 10'000,
 * meaning we know that only the 14 LSB will be used. However, shorts consist of at least 16 bits, so we can use the
 * 15-th bit to mark whether the bridge is red or not, artificially creating a larger cost.
 *
 * When calculating the total cost, it's important to apply the MASK_COST before adding the elements, in order to avoid
 * incorrect computations due to the MARK bit remaining.
 */
#define BRIDGE_MASK_COST 0x3FFF
#define BRIDGE_MARK_RED 0x1 << 14

/**
 * A bridge that can be built between two islands, at a given cost, and by a certain company.
 */
typedef struct bridge {
  int_fast32_t from, to;
  int_fast16_t cost;
} bridge_t;

// RADIX SORT FOR BRIDGES

#define RADIX_BITS   8                        // How many bits are in each byte.
#define RADIX_LEVELS (sizeof(int16_t))        // The number of radix iterations we have to perform. Only use 16 LSB.
#define RADIX_SIZE   (1 << RADIX_BITS)        // The number of bins for each radix step.
#define RADIX_MASK   (RADIX_SIZE - 1)         // The mask to apply on each radix step.

/**
 * Computes the frequencies of the different radix levels, incrementing the frequencies as appropriate based on the
 * radix level.
 *
 * @param m the number of bridges.
 * @param bridges the bridges for which we're computing frequencies.
 * @param frequencies the frequencies table to which we're writing.
 */
void radix_compute_frequencies(size_t m, bridge_t bridges[m], int frequencies[RADIX_LEVELS][RADIX_SIZE]) {
  for (int i = 0; i < m; ++i) {
    uint_fast16_t cost = bridges[i].cost;
    for (int l = 0; l < RADIX_LEVELS; ++l) {
      frequencies[l][cost & RADIX_MASK]++;
      cost = cost >> RADIX_BITS;
    }
  }
}

/**
 * Computes the indices table for the next radix pass. Indices indicate at which position of the output buffer the
 * items should be stored in order to fill the buffer in a tightly packed fashion.
 *
 * @param level the current level for which we're computing the indices.
 * @param frequencies the frequency counts.
 * @param indices the indices that will be returned.
 */
void radix_compute_indices(int level, int frequencies[RADIX_LEVELS][RADIX_SIZE], int indices[RADIX_LEVELS]) {
  int index = 0;
  for (int i = 0; i < RADIX_SIZE; i++) {
    indices[i] = index;
    index += frequencies[level][i];
  }
}

/**
 * Applies radix sorting to the given array of bridges. Radix may be considerably more cache-friendly than a
 * max-priority heap sort, hence why it might be preferred to obtain some good running times.
 *
 * @param m the number of bridges which are to be sorted.
 * @param bridges the bridges to sort.
 */
void radix_sort_increasing(size_t m, bridge_t bridges[m]) {
  if (m == 0) return;
  int frequencies[RADIX_LEVELS][RADIX_SIZE] = {0};
  int indices[RADIX_LEVELS] = {0};
  bridge_t buffer[m];

  bridge_t *from = bridges;
  bridge_t *to = buffer;

  radix_compute_frequencies(m, from, frequencies);
  for (int l = 0; l < RADIX_LEVELS; ++l) {
    radix_compute_indices(l, frequencies, indices);
    for (int i = 0; i < m; i++) {
      bridge_t bridge = from[i];
      int queue = (bridge.cost >> (l * RADIX_BITS)) & RADIX_MASK;
      to[indices[queue]++] = bridge;
    }
    // Swap from and to.
    bridge_t *tmp = from;
    from = to;
    to = tmp;
  }
  // If from is the original bridge, copy the results.
  if (bridges == to) memcpy(from, to, sizeof(bridge_t) * m);
}

/**
 * The result of the algorithm, returning the new happiness totals for blue and red bridges.
 */
typedef struct result {
  int red, blue;
} result_t;

result_t solve(int n, int m, bridge_t bridges[m]) {

  // Prepare our union-find data structure.
  uf_item_t uf[n];
  for (int i = 0; i < n; i++) {
    uf[i].parent = i;
    uf[i].rank = 0;
  }

  // Prepare the bridges queue.
  radix_sort_increasing(m, bridges);

  // Iterate over all the bridges, and compute the resulting sum !
  result_t result;
  result.blue = 0;
  result.red = 0;

  for (int i = m - 1; i >= 0; --i) {
    bridge_t bridge = bridges[i];
    int fr = uf_find(uf, bridge.from);
    int tr = uf_find(uf, bridge.to);
    if (fr != tr) {
      uf_union_r(uf, fr, tr);
      if ((bridge.cost & BRIDGE_MARK_RED) == BRIDGE_MARK_RED) {
        result.red += bridge.cost & BRIDGE_MASK_COST;
      } else {
        result.blue += bridge.cost;
      }
    }
  }

  return result;
}

#define BUFFER_SIZE (16 * 4096)

// A buffer large enough to store any line we're given.
char input_buffer[BUFFER_SIZE];
char *input_ptr = input_buffer;
char *input_ptr_end = input_buffer + BUFFER_SIZE - 1;

/**
 * Initialize the scanner with some proper values.
 */
void scan_init() {
  input_buffer[BUFFER_SIZE - 1] = '\0'; // Null-terminate the input buffer.
  fread(input_buffer, sizeof(char), BUFFER_SIZE - 1, stdin);
  input_ptr = input_buffer;
}

/** Parses the next multi-digit integer. */
int scan_int() {
  int n = 0;
  while (*input_ptr < '0' || *input_ptr > '9') {
    ++input_ptr;
    if (input_ptr == input_ptr_end) {
      size_t read = fread(input_buffer, sizeof(char), BUFFER_SIZE - 1, stdin);
      if (read == 0) input_buffer[0] = '\0';
      input_ptr = input_buffer;
    }
  }
  while (*input_ptr >= '0' && *input_ptr <= '9') {
    n *= 10;
    n += *input_ptr - '0';
    ++input_ptr;
    if (input_ptr == input_ptr_end) {
      size_t read = fread(input_buffer, sizeof(char), BUFFER_SIZE - 1, stdin);
      if (read == 0) input_buffer[0] = '\0';
      input_ptr = input_buffer;
    }
  }
  return n;
}

/** Parses the next character in range ['a', 'z']. */
char scan_char() {
  char c;
  while (*input_ptr < 'a' || *input_ptr > 'z') {
    ++input_ptr;
    if (input_ptr == input_ptr_end) {
      size_t read = fread(input_buffer, sizeof(char), BUFFER_SIZE - 1, stdin);
      if (read == 0) input_buffer[0] = '\0';
      input_ptr = input_buffer;
    }
  }
  c = *input_ptr;
  ++input_ptr;
  if (input_ptr == input_ptr_end) {
    size_t read = fread(input_buffer, sizeof(char), BUFFER_SIZE - 1, stdin);
    if (read == 0) input_buffer[0] = '\0';
    input_ptr = input_buffer;
  }
  return c;
}

int main() {

  scan_init();

  int n = scan_int();
  int m = scan_int();

  bridge_t bridges[m];

  for (int i = 0; i < m; i++) {
    int_fast32_t from = (int_fast32_t) scan_int();
    int_fast32_t to = (int_fast32_t) scan_int();
    int_fast16_t cost = (int_fast16_t) scan_int();
    char company = scan_char();

    bridges[i].from = from - 1;
    bridges[i].to = to - 1;
    bridges[i].cost = cost;
    if (company == 'r') bridges[i].cost |= BRIDGE_MARK_RED;
  }

  result_t result = solve(n, m, bridges);
  printf("%d %d\n", result.red, result.blue);
  return 0;
}
