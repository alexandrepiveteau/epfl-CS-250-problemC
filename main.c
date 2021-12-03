#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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
  int from, to;
  int cost;
} bridge_t;

// MAX-PRIORITY QUEUE WITH HEAPS

// TODO : Inline this function.
/**
 * Swaps two bridges from the provided array, located at indices a and b. This will work even if a and b are equal.
 *
 * @param bridges the bridges array.
 * @param a the index of the first bridge.
 * @param b the index of the second bridge.
 */
inline void bridge_swap(bridge_t bridges[], size_t a, size_t b) {
  bridge_t tmp = bridges[a];
  bridges[a] = bridges[b];
  bridges[b] = tmp;
}

/**
 * Builds a max heap, using the cost property of bridges.
 *
 * @param n the number of bridges in the array.
 * @param from the index at which the heapify procedure starts.
 * @param bridges the bridges array.
 */
inline void heap_max_heapify(const size_t n, size_t from, bridge_t bridges[n]) {
  while (true) {
    size_t left = 2 * from + 1;
    size_t right = left + 1;
    size_t largest = from;
    if (left < n && bridges[left].cost > bridges[largest].cost) largest = left;
    if (right < n && bridges[right].cost > bridges[largest].cost) largest = right;
    if (largest == from) return;
    bridge_swap(bridges, from, largest);
    from = largest;
  }
}

/**
 * Builds a max heap from the provided bridges array.
 *
 * @param n the number of bridges in the array.
 * @param bridges the bridges array.
 */
inline void heap_build_max_heap(const size_t n, bridge_t bridges[n]) {
  for (int from = ((int) n / 2) - 1; from >= 0; --from) {
    heap_max_heapify(n, from, bridges);
  }
}

/**
 * Extracts the bridge with the maximum cost from the array. Because this removes an element, you should not forget
 * to decrement the size when this is called !
 *
 * @param n the number of bridges in the array.
 * @param bridges the bridges array.
 * @return the extracted maximum.
 */
bridge_t heap_extract_max(const size_t n, bridge_t bridges[n]) {
  bridge_t max = bridges[0];
  bridges[0] = bridges[n - 1];
  heap_max_heapify(n - 1, 0, bridges);
  return max;
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
  heap_build_max_heap(m, bridges);

  // Iterate over all the bridges, and compute the resulting sum !
  result_t result;
  result.blue = 0;
  result.red = 0;
  int total = 0;
  int remaining = m;

  while (total < n - 1 && remaining > 0) {
    bridge_t bridge = heap_extract_max(remaining--, bridges);
    int fr = uf_find(uf, bridge.from);
    int tr = uf_find(uf, bridge.to);
    if (fr != tr) {
      total++;
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
    int from = scan_int();
    int to = scan_int();
    int cost = scan_int();
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
