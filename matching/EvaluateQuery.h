#ifndef SUBGRAPHMATCHING_EVALUATEQUERY_H
#define SUBGRAPHMATCHING_EVALUATEQUERY_H

#include "utility/FiPE/FiPE.h"
#include <vector>
#include <queue>
#include <unordered_set>
#include <bitset>
#include <gmp.h>

class EvaluateQuery {
public:
    static void
    GeneralEngine(const Graph *data_graph, const Graph *query_graph, Edges ***edge_matrix, ui **candidates,
                  ui *candidates_count, ui *order, ui *pivot, size_t output_limit_num, size_t &call_count, mpz_t embedding_cnt,
                  int64_t& time_limit);

    static void
    FiPEEngine(const Graph *data_graph, const Graph *query_graph, Edges ***edge_matrix, ui **candidates,
               ui *candidates_count, size_t output_limit_num, size_t &call_count, mpz_t embedding_cnt,
               int64_t& time_limit);

private:
    static void generateBN(const Graph *query_graph, ui *order, ui *pivot, ui **&bn, ui *&bn_count);
    static void allocateBuffer(const Graph *query_graph, const Graph *data_graph, ui *candidates_count, ui *&idx,
                                   ui *&idx_count, ui *&embedding, ui *&idx_embedding, ui *&temp_buffer,
                                   ui **&valid_candidate_idx, bool *&visited_vertices);
    static void releaseBuffer(ui q_num, ui *idx, ui *idx_count, ui *embedding, ui *idx_embedding,
                                  ui *temp_buffer, ui **valid_candidate_idx, bool *visited_vertices, ui **bn, ui *bn_count);

    static void generateValidCandidateIndex(const Graph *data_graph, ui depth, ui *embedding, ui *idx_embedding,
                                            ui *idx_count, ui **valid_candidate_index, Edges ***edge_matrix,
                                            bool *visited_vertices, ui **bn, ui *bn_cnt, ui *order, ui *pivot,
                                            ui **candidates, const Graph *query_graph);

    static void FiPEEnum(FiPEIndex& index);

    static bool comSharedDis(FiPEIndex& index, ui depth);

    static bool comSharedCon(FiPEIndex& index, ui depth);

    static bool comCurSpace(FiPEIndex& index, ui depth);

    static bool setCurSpaceCon(FiPEIndex& index, ui depth);

    static void clearCurSpaceCon(FiPEIndex& index, ui depth);

    static void setCurSpaceDis(FiPEIndex& index, ui depth);

    static void clearCurSpaceDis(FiPEIndex& index, ui depth);

    static void comSub(FiPEIndex& index, ui depth);

    static void comEdgeSubCon(FiPEIndex& index, ui depth);

    static void comEdgeSubDis(FiPEIndex& index, ui depth);

    static bool comStartCans(FiPEIndex& index);

    inline static void splitCans(FiPEIndex& index, ui depth);
    inline static bool nxtSubCans(FiPEIndex& index, ui depth);
};


#endif //SUBGRAPHMATCHING_EVALUATEQUERY_H
