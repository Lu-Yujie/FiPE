#ifndef LU_COMMON_H
#define LU_COMMON_H

/**
 * some common structures
 */
#include <vector>
#include "graph/graph.h"
using namespace std;
typedef unsigned int ui;

/**
 * universal binary search for const ui* & const vector<ui>
 */
template <class T>
class b_search {
public:
  // small array: linear seach
  static ui smallArraySearch(const T &arr, ui size, ui target) {
    for (ui i = 0; i < size; ++i) {
      if (arr[i] == target)
        return i;
    }
    return static_cast<ui>(-1);
  }

  // large array, use lower bound func, for ui*
  static ui largeArraySearch(const ui* arr, ui size, ui target) {
    auto ptr = lower_bound(arr, arr + size, target);
    if (ptr != arr + size && *ptr == target)
      return ptr - arr;
    else
      return (ui)-1;
  }

  // large array, use lower bound
  static ui largeArraySearch(const vector<ui> &arr, ui size, ui target) {
    auto ptr = std::lower_bound(arr.begin(), arr.begin() + size, target);
    if (ptr != arr.begin() + size && *ptr == target)
      return ptr - arr.begin();
    else
      return static_cast<ui>(-1);
  }

  // Universal search method
  static ui search(const T &arr, ui size, ui target) {
    // Compile-time type check
    static_assert(std::is_same<T, std::vector<ui>>::value || std::is_same<T, ui*>::value,
                  "T must be either ui* or std::vector<ui>");
    if (size <= 4) {
      return smallArraySearch(arr, size, target);
    }
    return largeArraySearch(arr, size, target);
  }
}; // class b_search

/**
 * Embedding info
 * mapping between depth, u(query node), v(data node)
 * u2v: u->v, each u only match to one v
 * v2depth: v->depth, too much v, use map instead of array
 * depth2u: depth->u, use vector for dynamic tree height
 */
class Embedding {
public:
  VertexID *u2v;            // u->v, use the 1st v of batch
  vector<VertexID> depth2u; // depth->u

  Embedding(ui cnt) {
    u2v = new VertexID[cnt];
    depth2u.reserve(cnt);
  }
  ~Embedding() {
    delete[] u2v;
  }
};

namespace peak_mem {
  /**
   * get peak virtual memory space of the current process
   * https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process#answer-64166
   */
  inline int parseLine(char *line) {
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char *p = line;
    while (*p < '0' || *p > '9')
      p++;
    line[i - 3] = '\0';
    i = atoi(p);
    return i;
  }

  inline int getValue() { // Note: this value is in KB!
    FILE *file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL) {
      if (strncmp(line, "VmPeak:", 7) == 0) {
        result = parseLine(line);
        break;
      }
    }
    fclose(file);
    return result;
  }
}

#endif
