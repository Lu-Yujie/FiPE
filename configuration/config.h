#ifndef CONFIG_H
#define CONFIG_H

/**
 * Setting the value as 1 is to (1) enable the neighbor label frequency filter (i.e., NLF filter); and (2) enable
 * to check the existence of an edge with the label information. The cost is to (1) build an unordered_map for each
 * vertex to store the frequency of the labels of its neighbor; and (2) build the label neighbor offset.
 * If the memory can hold the extra memory cost, then enable this feature to boost the performance. Otherwise, disable
 * it by setting this value as 0.
 */
#define OPTIMIZED_VLABELED_GRAPH 1

/**
 * Define ANALYZE_PEAK_MEMORY to analyze peak memory consumption
 */
// #define ANALYZE_PEAK_MEMORY

/**
 * Define ANALYZE_FUNC_MEMORY to analyze function memory consumption
 */
//  #define ANALYZE_FUNC_MEMORY

/**
 * Define ANALYZE_DUPLICATE to enable the record the duplicate information
 */
// #define ANALYZE_DUPLICATE

/**
 * Define ANALYZE_TIME to analyze time consumption of each segment
 */
// #define ANALYZE_TIME

/**
 * Define minimal subset of candidates for backtracking
 */
#define MIN_SUBCANS 16

#define PRINT_SEPARATOR "------------------------------"

#endif //SUBGRAPHMATCHING_CONFIG_H
