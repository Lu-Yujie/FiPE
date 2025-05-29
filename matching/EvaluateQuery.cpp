#include "EvaluateQuery.h"
#include <stack>
#include <vector>
#include <cstring>
#include <sys/stat.h>
#include <fstream>

void EvaluateQuery::generateBN(const Graph *query_graph, ui *order, ui *pivot, ui **&bn, ui *&bn_count) {
    ui q_num = query_graph->getVerticesCount();
    bn_count = new ui[q_num];
    std::fill(bn_count, bn_count + q_num, 0);
    bn = new ui *[q_num];
    for (ui i = 0; i < q_num; ++i) {
        bn[i] = new ui[q_num];
    }

    std::vector<bool> visited_vertices(q_num, false);
    visited_vertices[order[0]] = true;
    for (ui i = 1; i < q_num; ++i) {
        VertexID vertex = order[i];

        ui nbrs_cnt;
        const ui *nbrs = query_graph->getVertexNeighbors(vertex, nbrs_cnt);
        for (ui j = 0; j < nbrs_cnt; ++j) {
            VertexID nbr = nbrs[j];

            if (visited_vertices[nbr] && nbr != pivot[i]) {
                bn[i][bn_count[i]++] = nbr;
            }
        }

        visited_vertices[vertex] = true;
    }
}

void
EvaluateQuery::GeneralEngine(const Graph *data_graph, const Graph *query_graph, Edges ***edge_matrix,
                             ui **candidates, ui *candidates_count, ui *order, ui *pivot,
                             size_t output_limit_num, size_t &call_count, mpz_t embedding_cnt,
                             int64_t& time_limit) {
    // Generate the bn.
    ui **bn;
    ui *bn_count;
    generateBN(query_graph, order, pivot, bn, bn_count);

    // Allocate the memory buffer.
    ui *idx;
    ui *idx_count;
    ui *embedding;
    ui *idx_embedding;
    ui *temp_buffer;
    ui **valid_candidate_idx;
    bool *visited_vertices;
    allocateBuffer(data_graph, query_graph, candidates_count, idx, idx_count, embedding, idx_embedding,
                   temp_buffer, valid_candidate_idx, visited_vertices);
    // Evaluate the query.
    mpz_init_set_ui(embedding_cnt, 0);
    ui cur_depth = 0;
    ui max_depth = query_graph->getVerticesCount();
    VertexID start_vertex = order[0];

#ifdef ANALYZE_DUPLICATE
    auto g_name = query_graph->duplicate_path;
    int status = mkdir(g_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    if (status != 0) {
        std::cout << g_name << ": Failed to create folder.\n";
        exit(-1);
    }

    std::vector<std::ofstream> out_files;
    out_files.resize(max_depth);
    for (ui i = 1; i < max_depth; i++) {
        out_files[i-1].open(g_name + "/" + std::to_string(i-1) + ".txt");
    }
    memset(embedding, (ui)-1, sizeof(ui)*max_depth);
    std::vector<std::stack<std::vector<ui>>> valid_cans;
    std::vector<ui> level_cnt(max_depth, 0);  // record the number of subtree at each level
    for (ui i = 0; i < max_depth; i++) {
        std::stack<std::vector<ui>> sv;
        std::vector<ui> vec;
        vec.insert(vec.end(), candidates[i], candidates[i]+candidates_count[i]);
        sv.push(move(vec));
        valid_cans.emplace_back(move(sv));
    }
#endif

    idx[cur_depth] = 0;
    idx_count[cur_depth] = candidates_count[start_vertex];

    for (ui i = 0; i < idx_count[cur_depth]; ++i) {
        valid_candidate_idx[cur_depth][i] = i;
    }

    while (true) {
        while (idx[cur_depth] < idx_count[cur_depth]) {
            if (TimeOp::getClockNan() >= time_limit) {
                goto EXIT;
            }
            ui valid_idx = valid_candidate_idx[cur_depth][idx[cur_depth]];
            VertexID u = order[cur_depth];
            VertexID v = candidates[u][valid_idx];

            embedding[u] = v;
            idx_embedding[u] = valid_idx;
            visited_vertices[v] = true;
            idx[cur_depth] += 1;

            if (cur_depth == max_depth - 1) {
                mpz_add_ui(embedding_cnt, embedding_cnt, 1);
                visited_vertices[v] = false;
                if (output_limit_num != (size_t)-1 && mpz_cmp_ui(embedding_cnt, output_limit_num) > 0) {
                    goto EXIT;
                }
            } else {
                call_count += 1;
                cur_depth += 1;
                idx[cur_depth] = 0;
                generateValidCandidateIndex(data_graph, cur_depth, embedding, idx_embedding, idx_count,
                                            valid_candidate_idx, edge_matrix, visited_vertices, bn,
                                            bn_count, order, pivot, candidates, query_graph);
#ifdef ANALYZE_DUPLICATE
                // compute valid_cans of all u's connected to u
                level_cnt[cur_depth]++;
                ui unbrs_cnt = 0;
                auto unbrs = query_graph->getVertexNeighbors(u, unbrs_cnt);
                for (ui i = 0; i < unbrs_cnt; i++) {
                    auto unbr = unbrs[i];
                    if (embedding[unbr] != (ui)-1) {  // if not matched, compute valid_cans
                        // first, get all neighbors of v on dataGraph
                        ui vnbrs_cnt = 0;
                        auto vnbrs = data_graph->getVertexNeighbors(v, vnbrs_cnt);
                        // and then intersected with valid_cans[unbr] & push
                        auto res = SetOp::intersectTwo(valid_cans[unbr].top(), vnbrs, vnbrs_cnt);
                        valid_cans[unbr].push(move(res));
                    }
                }
                out_files[cur_depth-1] << level_cnt[cur_depth-1] << std::endl;
                for (ui i = cur_depth; i < max_depth; i++) {
                    auto cur_u = order[i];
                    for (auto& can : valid_cans[i].top()) {
                        out_files[cur_depth-1] << can << " ";
                    }
                    out_files[cur_depth-1] << std::endl;
                }
                out_files[cur_depth-1] << "------" << std::endl;
#endif
            }
        }

        // backtrack
        cur_depth -= 1;
        if (cur_depth == (ui)-1)
            break;
        else
            visited_vertices[embedding[order[cur_depth]]] = false;
#ifdef ANALYZE_DUPLICATE
        // if (idx_count[cur_depth+1] == 0) continue;
        auto last_u = order[cur_depth + 1];
        embedding[last_u] = (ui)-1;
        auto u = order[cur_depth];
        auto v = embedding[u];
        // restore of neighbors of u valid_cans
        ui unbrs_cnt = 0;
        auto unbrs = query_graph->getVertexNeighbors(u, unbrs_cnt);
        for (ui i = 0; i < unbrs_cnt; i++) {
            auto unbr = unbrs[i];
            if (embedding[unbr] != (ui)-1) {
                valid_cans[unbr].pop();
            }
        }
#endif
    }


    // Release the buffer.
    EXIT:
    releaseBuffer(max_depth, idx, idx_count, embedding, idx_embedding, temp_buffer, valid_candidate_idx,
                  visited_vertices,
                  bn, bn_count);
#ifdef ANALYZE_DUPLICATE
    for (ui i = 1; i < max_depth; i++) { 
        out_files[i-1].close();
    }
#endif

    return;
}

void
EvaluateQuery::allocateBuffer(const Graph *data_graph, const Graph *query_graph, ui *candidates_count, ui *&idx,
                              ui *&idx_count, ui *&embedding, ui *&idx_embedding, ui *&temp_buffer,
                              ui **&valid_candidate_idx, bool *&visited_vertices) {
    ui q_num = query_graph->getVerticesCount();
    ui d_num = data_graph->getVerticesCount();
    ui max_candidates_num = candidates_count[0];

    for (ui i = 1; i < q_num; ++i) {
        VertexID cur_vertex = i;
        ui cur_candidate_num = candidates_count[cur_vertex];

        if (cur_candidate_num > max_candidates_num) {
            max_candidates_num = cur_candidate_num;
        }
    }

    idx = new ui[q_num];
    idx_count = new ui[q_num];
    embedding = new ui[q_num];
    idx_embedding = new ui[q_num];
    visited_vertices = new bool[d_num];
    temp_buffer = new ui[max_candidates_num];
    valid_candidate_idx = new ui *[q_num];
    for (ui i = 0; i < q_num; ++i) {
        valid_candidate_idx[i] = new ui[max_candidates_num];
    }

    std::fill(visited_vertices, visited_vertices + d_num, false);
}

void EvaluateQuery::generateValidCandidateIndex(const Graph *data_graph, ui depth, ui *embedding, ui *idx_embedding,
                                                ui *idx_count, ui **valid_candidate_index, Edges ***edge_matrix,
                                                bool *visited_vertices, ui **bn, ui *bn_cnt, ui *order, ui *pivot,
                                                ui **candidates, const Graph *query_graph) {
    VertexID u = order[depth];
    VertexID pivot_vertex = pivot[depth];
    ui idx_id = idx_embedding[pivot_vertex];
    Edges &edge = *edge_matrix[pivot_vertex][u];
    ui count = edge.offset_[idx_id + 1] - edge.offset_[idx_id];
    ui *candidate_idx = edge.edge_ + edge.offset_[idx_id];

    ui valid_candidate_index_count = 0;

    if (bn_cnt[depth] == 0) {
        for (ui i = 0; i < count; ++i) {
            ui temp_idx = candidate_idx[i];
            VertexID temp_v = candidates[u][temp_idx];

            if (!visited_vertices[temp_v])
                valid_candidate_index[depth][valid_candidate_index_count++] = temp_idx;
        }
    } else {
        for (ui i = 0; i < count; ++i) {
            ui temp_idx = candidate_idx[i];
            VertexID temp_v = candidates[u][temp_idx];

            if (!visited_vertices[temp_v]) {
                bool valid = true;

                for (ui j = 0; j < bn_cnt[depth]; ++j) {
                    VertexID u_bn = bn[depth][j];
                    VertexID u_bn_v = embedding[u_bn];
                    if (!data_graph->checkEdgeExistence(temp_v, u_bn_v)) {
                        valid = false;
                        break;
                    }
                }

                if (valid)
                    valid_candidate_index[depth][valid_candidate_index_count++] = temp_idx;
            }
        }
    }

    idx_count[depth] = valid_candidate_index_count;
}

void EvaluateQuery::releaseBuffer(ui q_num, ui *idx, ui *idx_count, ui *embedding, ui *idx_embedding,
                                  ui *temp_buffer, ui **valid_candidate_idx, bool *visited_vertices, ui **bn,
                                  ui *bn_count) {
    delete[] idx;
    delete[] idx_count;
    delete[] embedding;
    delete[] idx_embedding;
    delete[] visited_vertices;
    delete[] bn_count;
    delete[] temp_buffer;
    for (ui i = 0; i < q_num; ++i) {
        delete[] valid_candidate_idx[i];
        delete[] bn[i];
    }

    delete[] valid_candidate_idx;
    delete[] bn;
}

#ifdef ANALYZE_TIME
int64_t FiPEIndex::refine_time = 0;
int64_t FiPEIndex::enumerate_time = 0;
int64_t FiPEIndex::getNeighbors_time = 0;
#endif

vector<ui> EdgeSub::down_record;

/**
 * use FiPE method
*/
void
EvaluateQuery::FiPEEngine(const Graph *data_graph, const Graph *query_graph, Edges ***edge_matrix,
                          ui **candidates, ui *candidates_count, size_t output_limit_num,
                          size_t &call_count, mpz_t embedding_cnt, int64_t& time_limit) {
#ifdef ANALYZE_TIME
    auto total_time = TimeOp::getClockNan();
#endif
    auto qnum = query_graph->getVerticesCount();

    // separate indep and cover vertices(min_vertex_cover)
    VertexID* order = new VertexID[qnum];
    // ui num_cover = FiPEIndep::indepSetOnCans(query_graph, order, candidates_count);
    ui num_cover = FiPEIndep::indepSetOnDegree(query_graph, order);
    // main data structure
    FiPEIndex index(query_graph, data_graph, edge_matrix, candidates, candidates_count, num_cover, order);
    EdgeSub::down_record.resize(data_graph->getVerticesCount(), (ui)-1);

    // auxiliary data structure
    mpz_init_set_ui(embedding_cnt, 0);
    ui cur_depth = 0;
    VertexID start_vertex = order[cur_depth];
    auto& level_embeddings = index.indepInfo->embedding_total;
    auto& subInfo = index.subInfo_;

#ifdef ANALYZE_DUPLICATE
    auto g_name = query_graph->duplicate_path;
    int status = mkdir(g_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    if (status != 0) {
        std::cout << g_name << ": Failed to create folder.\n";
        exit(-1);
    }

    std::vector<std::ofstream> out_files;
    out_files.resize(qnum);
    for (ui i = 1; i < qnum; i++) {
        out_files[i-1].open(g_name + "/" + std::to_string(i-1) + ".txt");
    }

    // supplement the full order
    ui num_indep = 0;
    for (VertexID cur_u = 0; cur_u < qnum; cur_u++) {
        if (index.indepInfo->indep_bool[cur_u]) {
            order[num_cover + num_indep++] = cur_u;
        }
    }
#endif

    splitCans(index, cur_depth);
    while (!comStartCans(index) || !comCurSpace(index, cur_depth)) {
        if (nxtSubCans(index, cur_depth) == false) goto EXIT;
    }
    comSub(index, cur_depth);

    while (true) {
        while (subInfo[cur_depth].subs.cur_s < subInfo[cur_depth].subs.c_cnt) {
            if (TimeOp::getClockNan() >= time_limit) {
                goto EXIT;
            }
            VertexID u = order[cur_depth];

            if (subInfo[cur_depth].connected) setCurSpaceCon(index, cur_depth);
            else setCurSpaceDis(index, cur_depth);
            subInfo[cur_depth].subs.cur_s++;

#ifdef ANALYZE_DUPLICATE
            for (ui i = cur_depth+1; i < qnum; i++) {
                auto cur_u = order[i];
                for (ui j = 0; j < index.valid_cans_[cur_u].back().size();j++) {
                    out_files[cur_depth] << index.valid_cans_[cur_u].back()[j] << " ";
                }
                out_files[cur_depth] << std::endl;
            }
            out_files[cur_depth] << "------" << std::endl;
#endif

            if (cur_depth >= num_cover - 2) {
#ifdef ANALYZE_DUPLICATE
                for (ui indep_idx = num_cover-1; indep_idx < qnum - 1; indep_idx++) {
                    for (VertexID cur_u = 0; cur_u < qnum; cur_u++) {
                        if (!index.indepInfo->indep_bool[cur_u]) continue;
                        for (ui j = 0; j < index.valid_cans_[cur_u].back().size();j++) {
                            out_files[indep_idx] << index.valid_cans_[cur_u].back()[j] << " ";
                        }
                        out_files[indep_idx] << std::endl;
                    }
                    out_files[indep_idx] << "------" << std::endl;
                }
#endif
                // enumerate results on indep nodes, process ancestors' ves by the way
                FiPEEnum(index);
                mpz_add(embedding_cnt, embedding_cnt, level_embeddings);
                // nxt batch
                if (subInfo[cur_depth].connected) clearCurSpaceCon(index, cur_depth);
                else clearCurSpaceDis(index, cur_depth);
            } else {
                // if failed on nxt vertex, try nxt can of last u
                cur_depth++;
                splitCans(index, cur_depth);
                bool nxt_valid = true;
                while (!comCurSpace(index, cur_depth)) {
                    if (!nxtSubCans(index, cur_depth)) {
                        index.subCans_[cur_depth+1].splitted = false;
                        nxt_valid = false;
                        break;
                    }
                }
                if (!nxt_valid) break;
                comSub(index, cur_depth);
                call_count++;
            }
        }

        // backtracking
        bool nxt_valid = true;
        if (nxtSubCans(index, cur_depth)) {
            if (cur_depth == 0) {
                while (!comStartCans(index) || !comCurSpace(index, cur_depth)) {
                    if (!nxtSubCans(index, cur_depth)) {
                        nxt_valid = false;
                        break;
                    }
                }
            } else {
               while (!comCurSpace(index, cur_depth)) {
                    if (nxtSubCans(index, cur_depth) == false) {
                        nxt_valid = false;
                        break;
                    }
                }
            }
            if (nxt_valid) {
                comSub(index, cur_depth);
                continue;
            }
        }
        cur_depth--;
        if (cur_depth == ui(-1))
            break;
        if (subInfo[cur_depth].connected) clearCurSpaceCon(index, cur_depth);
        else clearCurSpaceDis(index, cur_depth);
    }

    // Release the buffer.
    EXIT:
#ifdef ANALYZE_TIME
    std::cout << "enumerate_time:    " << FiPEIndex::enumerate_time << std::endl;
    std::cout << "refine_time:       " << FiPEIndex::refine_time << std::endl;
    std::cout << "getNeighbors_time: " << FiPEIndex::getNeighbors_time << std::endl;
    std::cout << "total_time:        " << TimeOp::getClockNan() - total_time << std::endl;
#endif
#ifdef ANALYZE_DUPLICATE
    for (ui i = 1; i < qnum; i++) { 
        out_files[i-1].close();
    }
#endif
#ifdef ANALYZE_FUNC_MEMORY
    mem::printVmRSS("Engine");
#endif
    return;
}

/**
 * split cans when there are too much cans
 */
inline void
EvaluateQuery::splitCans(FiPEIndex& index, ui depth) {
    auto& down = index.order_[depth+1];
    auto& subCans = index.subCans_[depth+1];
    subCans.up_changed = true;
    if (depth == 0) {
        auto& up = index.order_[depth];
        auto& upCans = index.subCans_[depth];
        if (index.valid_cans_[up].back().size() > MIN_SUBCANS) {
            upCans.splitted = true;
            upCans.cur_start = 0;
            upCans.cur_end = MIN_SUBCANS;
            vector<VertexID> new_cans;
            new_cans.reserve(MIN_SUBCANS);
            new_cans.insert(new_cans.end(),
                            index.valid_cans_[up].back().begin(),
                            index.valid_cans_[up].back().begin()+MIN_SUBCANS);
            index.valid_cans_[up].push_back(move(new_cans));
        } else {
            upCans.splitted = false;
        }
    }
    // if too much candidates, split
    if (!index.subInfo_[depth].connected
        && index.valid_cans_[down].back().size() > MIN_SUBCANS) {
        subCans.splitted = true;
        subCans.cur_start = 0;
        subCans.cur_end = MIN_SUBCANS;
        vector<VertexID> new_cans;
        new_cans.reserve(MIN_SUBCANS);
        new_cans.insert(new_cans.end(),
                        index.valid_cans_[down].back().begin(),
                        index.valid_cans_[down].back().begin()+MIN_SUBCANS);
        index.valid_cans_[down].push_back(move(new_cans));
    } else {
        subCans.splitted = false;
    }
    return;
}

/**
 * nxt sub cans
 * false->splited but have no next subset of cans, true->o.w.
 */
inline bool
EvaluateQuery::nxtSubCans(FiPEIndex& index, ui depth) {
    auto& down = index.order_[depth+1];
    auto& subCans = index.subCans_[depth+1];
    subCans.up_changed = false;
    if (subCans.splitted) index.valid_cans_[down].pop_back();
    if (depth == 0) {
        auto& up = index.order_[depth];
        auto& upCans = index.subCans_[depth];
        if (subCans.splitted == false
            || subCans.cur_end == index.valid_cans_[down].back().size()) {
            if (upCans.splitted) index.valid_cans_[up].pop_back();
            if (upCans.splitted == false
                || upCans.cur_end == index.valid_cans_[up].back().size()) {
                return false;
            }
            upCans.cur_start = upCans.cur_end;
            upCans.cur_end = min(upCans.cur_end + MIN_SUBCANS,
                                 (ui)index.valid_cans_[up].back().size());
            vector<VertexID> new_cans;
            new_cans.reserve(MIN_SUBCANS);
            new_cans.insert(new_cans.end(),
                            index.valid_cans_[up].back().begin()+upCans.cur_start,
                            index.valid_cans_[up].back().begin()+upCans.cur_end);
            index.valid_cans_[up].push_back(move(new_cans));
            subCans.up_changed = true;
            subCans.cur_start = 0;
            subCans.cur_end = MIN_SUBCANS;
        } else {
            subCans.cur_start = subCans.cur_end;
            subCans.cur_end = min(subCans.cur_end + MIN_SUBCANS,
                                  (ui)index.valid_cans_[down].back().size());
        }
    } else {
        if (subCans.splitted == false
            || subCans.cur_end == index.valid_cans_[down].back().size())
            return false;
        subCans.cur_start = subCans.cur_end;
        subCans.cur_end = min(subCans.cur_end + MIN_SUBCANS,
                              (ui)index.valid_cans_[down].back().size());
    }
    if (subCans.splitted) {
        vector<VertexID> new_cans;
        new_cans.reserve(MIN_SUBCANS);
        new_cans.insert(new_cans.end(),
                        index.valid_cans_[down].back().begin()+subCans.cur_start,
                        index.valid_cans_[down].back().begin()+subCans.cur_end);
        index.valid_cans_[down].push_back(move(new_cans));
    }
    return true;
}

/**
 * 1.set valid_cans of cur_s
 * 3.push valid_cans from inf_cans to FiPEIndex
 * 1.generate sub_cans for next_u
 *   1.1 connected: union of nbrs from (sub_cans of u) to next_u
 *   1.2 o.w.: u will not influence the cans of next_u
 * 2.compute the valid edges for current u & u_n-1(if conncected)
 * return: false, if valid_cans of delayed_nbrs is empty
*/
bool
EvaluateQuery::setCurSpaceCon(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& subs = subInfo.subs;
    auto& down_cans = subInfo.down_cans;  // down_idx->down_cans
    auto& down_idxs = subInfo.down_idxs;  // edge_idx->down_idx
    auto& edge_up_idxs = subs.edge_up_idxs;
    auto& edge_down_idxs = subs.edge_down_idxs;
    auto& cur_s = subs.cur_s;
    auto& edge_up_start = subs.edge_up_offset[cur_s];
    auto& edge_up_end = subs.edge_up_offset[cur_s+1];
    auto& edge_down_start = subs.edge_down_offset[edge_up_start];
    auto& edge_down_end = subs.edge_down_offset[edge_up_end];
    auto& down_record = subs.down_record;
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;
    auto& valid_cans = index.valid_cans_;

    if (depth < index.indepInfo->num_cover_-2) {
        // compute up_cans of nxtSub
        auto& nxtSub = index.subInfo_[depth+1];
        auto& nxt_up_cans = nxtSub.up_cans;
        nxt_up_cans.clear();
        auto& nxt_up_idxs = subInfo.nxt_up_idxs;
        auto& last_down_idxs = nxtSub.last_down_idxs;
        nxt_up_idxs.resize(down_cans.size());
        last_down_idxs.clear();
        for (ui i = edge_down_start; i < edge_down_end; i++) {
            auto& edge_idx = edge_down_idxs[i];
            auto& down_idx = down_idxs[edge_idx];  // idx of influenced
            auto& down = down_cans[down_idx];
            if (down_record[down] == (ui)-1) {
                down_record[down] = nxt_up_cans.size();
                nxt_up_cans.emplace_back(down);
                last_down_idxs.emplace_back(down_idx);
                nxt_up_idxs[down_idx] = down_record[down];
            }
        }
        for (auto& nxt_up_can : nxt_up_cans) down_record[nxt_up_can] = (ui)-1;

        // compute the valid_cans of delayed_nbrs
        auto& delayed_nbrs = subInfo.nbrs.delayed_;
        subInfo.delayed_inf.clear();
        subInfo.delayed_inf.resize(delayed_nbrs.size(), true);
        for (ui i = 0; i < delayed_nbrs.size(); i++) {
            auto& delayed_nbr = delayed_nbrs[i];
            // if any can do not influence shared_nbr, valid_cans keeps
            for (ui j = 0; j < nxt_up_cans.size(); j++) {
                auto& down_idx = last_down_idxs[j];
                if (!influenced[delayed_nbr][down_idx]) {
                    subInfo.delayed_inf[i] = false;
                    break;
                }
            }
            if (subInfo.delayed_inf[i]) {
                vector<vector<VertexID>> all_cans;
                all_cans.reserve(nxt_up_cans.size());
                for (ui j = 0; j < nxt_up_cans.size(); j++) {
                    auto& down_idx = last_down_idxs[j];
                    all_cans.emplace_back(inf_cans[delayed_nbr][down_idx]);
                }
                valid_cans[delayed_nbr].push_back(move(SetOp::unionMultiple(all_cans)));
            }
        }
    }

    // push valid_cans from inf_cans to FiPEIndex
    // use the first as the representative
    auto& up_idx = edge_up_idxs[edge_up_start];
    for (auto& nbr : subInfo.nbrs.up_indep_) {
        if (influenced[nbr][up_idx]) {
            valid_cans[nbr].push_back(inf_cans[nbr][up_idx]);
        }
    }
    auto& edge_idx = edge_down_idxs[edge_down_start];
    for (auto& nbr : subInfo.nbrs.shared_) {
        if (influenced[nbr][edge_idx]) {
            valid_cans[nbr].push_back(move(inf_cans[nbr][edge_idx]));
        }
    }
    auto down_idx = down_idxs[edge_idx];
    for (auto& nbr : subInfo.nbrs.down_indep_) {
        if (influenced[nbr][down_idx]) {
            valid_cans[nbr].push_back(inf_cans[nbr][down_idx]);
        }
    }
    return true;
}

/**pop the valid_cans of influenced_u from FiPEIndex
*/
void
EvaluateQuery::clearCurSpaceCon(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& subs = subInfo.subs;
    auto& down_idxs = subInfo.down_idxs;  // edge_idx->down_idx
    auto& edge_up_idxs = subs.edge_up_idxs;
    auto& edge_down_idxs = subs.edge_down_idxs;
    auto& cur_s = subs.cur_s;
    auto& edge_up_start = subs.edge_up_offset[cur_s-1];
    auto& edge_down_start = subs.edge_down_offset[edge_up_start];
    auto& influenced = subInfo.influenced;
    auto& valid_cans = index.valid_cans_;
    auto& delayed_inf = subInfo.delayed_inf;
    // pop valid_cans from inf_cans to FiPEIndex
    // use the first as the representative
    auto& up_idx = edge_up_idxs[edge_up_start];
    for (auto& nbr : subInfo.nbrs.up_indep_) {
        if (influenced[nbr][up_idx]) {
            valid_cans[nbr].pop_back();
        }
    }
    auto& edge_idx = edge_down_idxs[edge_down_start];
    for (auto& nbr : subInfo.nbrs.shared_) {
        if (influenced[nbr][edge_idx]) {
            valid_cans[nbr].pop_back();
        }
    }
    auto down_idx = down_idxs[edge_idx];
    for (auto& nbr : subInfo.nbrs.down_indep_) {
        if (influenced[nbr][down_idx]) {
            valid_cans[nbr].pop_back();
        }
    }
    for (ui i = 0; i < subInfo.nbrs.delayed_.size(); i++) {
        if (delayed_inf[i]) {
            valid_cans[subInfo.nbrs.delayed_[i]].pop_back();
        }
    }
}

void
EvaluateQuery::setCurSpaceDis(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& subs = subInfo.subs;
    auto& down_cans = subInfo.down_cans;  // down_idx->down_cans
    auto& edge_down_idxs = subs.edge_down_idxs;
    auto& cur_s = subs.cur_s;
    auto& edge_down_start = subs.edge_down_offset[cur_s];
    auto& edge_down_end = subs.edge_down_offset[cur_s+1];
    auto& down_record = subs.down_record;
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;
    auto& valid_cans = index.valid_cans_;
    ui down_num = down_cans.size();

    if (depth < index.indepInfo->num_cover_-2) {
        // compute up_cans of nxtSub
        auto& nxtSub = index.subInfo_[depth+1];
        auto& nxt_up_cans = nxtSub.up_cans;
        auto& nxt_up_idxs = subInfo.nxt_up_idxs;
        auto& last_down_idxs = nxtSub.last_down_idxs;
        nxt_up_idxs.resize(down_cans.size());
        last_down_idxs.clear();
        nxt_up_cans.clear();
        for (ui i = edge_down_start; i < edge_down_end; i++) {
            auto& edge_idx = edge_down_idxs[i];  // idx of influenced(shared)
            ui down_idx = edge_idx%down_num;       // idx of influenced(indep)
            auto& down = down_cans[down_idx];
            if (down_record[down] == (ui)-1) {
                down_record[down] = nxt_up_cans.size();
                nxt_up_cans.emplace_back(down);
                last_down_idxs.emplace_back(down_idx);
                nxt_up_idxs[down_idx] = down_record[down];
            }
        }
        for (auto& nxt_up_can : nxt_up_cans) down_record[nxt_up_can] = (ui)-1;

        // compute the valid_cans of delayed_nbrs
        auto& delayed_nbrs = subInfo.nbrs.delayed_;
        subInfo.delayed_inf.clear();
        subInfo.delayed_inf.resize(delayed_nbrs.size(), true);
        for (ui i = 0; i < delayed_nbrs.size(); i++) {
            auto& delayed_nbr = delayed_nbrs[i];
            // if any can do not influence shared_nbr, valid_cans keeps
            subInfo.delayed_inf[i] = true;
            for (ui j = 0; j < nxt_up_cans.size(); j++) {
                auto& down_idx = last_down_idxs[j];
                if (!influenced[delayed_nbr][down_idx]) {
                    subInfo.delayed_inf[i] = false;
                    break;
                }
            }
            if (subInfo.delayed_inf[i]) {
                vector<vector<VertexID>> all_cans;
                all_cans.reserve(nxt_up_cans.size());
                for (ui j = 0; j < nxt_up_cans.size(); j++) {
                    auto& down_idx = last_down_idxs[j];
                    all_cans.emplace_back(inf_cans[delayed_nbr][down_idx]);
                }
                valid_cans[delayed_nbr].push_back(move(SetOp::unionMultiple(all_cans)));
            }
        }
    }

    // push valid_cans from inf_cans to FiPEIndex
    // use the first as the representative
    auto& edge_idx = edge_down_idxs[edge_down_start];
    auto up_idx = edge_idx/down_num;
    auto down_idx = edge_idx%down_num;
    for (auto& nbr : subInfo.nbrs.up_indep_) {
        if (influenced[nbr][up_idx]) {
            valid_cans[nbr].push_back(inf_cans[nbr][up_idx]);
        }
    }
    for (auto& nbr : subInfo.nbrs.shared_) {
        if (influenced[nbr][edge_idx]) {
            valid_cans[nbr].push_back(move(inf_cans[nbr][edge_idx]));
        }
    }
    for (auto& nbr : subInfo.nbrs.down_indep_) {
        if (influenced[nbr][down_idx]) {
            valid_cans[nbr].push_back(inf_cans[nbr][down_idx]);
        }
    }
}

/**pop the valid_cans of influenced_u from FiPEIndex
*/
void
EvaluateQuery::clearCurSpaceDis(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& subs = subInfo.subs;
    auto& down_cans = subInfo.down_cans;
    auto& edge_down_idxs = subs.edge_down_idxs;
    auto& cur_s = subs.cur_s;
    auto& edge_down_start = subs.edge_down_offset[cur_s-1];
    auto& influenced = subInfo.influenced;
    auto& valid_cans = index.valid_cans_;
    auto& delayed_inf = subInfo.delayed_inf;
    ui down_num = down_cans.size();
    // use the first as the representative
    auto& edge_idx = edge_down_idxs[edge_down_start];
    auto up_idx = edge_idx/down_num;
    auto down_idx = edge_idx%down_num;
    for (auto& nbr : subInfo.nbrs.up_indep_) {
        if (influenced[nbr][up_idx]) {
            valid_cans[nbr].pop_back();
        }
    }
    for (auto& nbr : subInfo.nbrs.shared_) {
        if (influenced[nbr][edge_idx]) {
            valid_cans[nbr].pop_back();
        }
    }
    for (auto& nbr : subInfo.nbrs.down_indep_) {
        if (influenced[nbr][down_idx]) {
            valid_cans[nbr].pop_back();
        }
    }
    for (ui i = 0; i < subInfo.nbrs.delayed_.size(); i++) {
        if (delayed_inf[i]) {
            valid_cans[subInfo.nbrs.delayed_[i]].pop_back();
        }
    }
}

void
EvaluateQuery::FiPEEnum(FiPEIndex& index) {
#ifdef ANALYZE_TIME
    auto start_time = TimeOp::getClockNan();
#endif
    auto qnum = index.q_graph_->getVerticesCount();
    auto dnum = index.d_graph_->getVerticesCount();
    auto& order = index.order_;
    auto& used_cans = index.indepInfo->used_cans_;
    memset(used_cans, false, sizeof(bool)*dnum);
    auto& embedding_total = index.indepInfo->embedding_total;
    mpz_set_ui(embedding_total, 0);
    auto& embedding_uncon = index.indepInfo->embedding_uncon;
    mpz_set_ui(embedding_uncon, 0);
    auto& uncon = index.indepInfo->uncon;
    uncon = false;
    auto& embedding_step = index.indepInfo->embedding_step;
    auto& cans = index.indepInfo->cans;
    auto& visited_v = index.visited_v;
    auto& subInfo = index.subInfo_;

    // extract the candidates of indep vertices
    for (VertexID i = 0; i < qnum; i++) {
        if (index.indepInfo->indep_bool[i]) {
            cans[i] = index.valid_cans_[i].back();
            for (auto& can:cans[i]) used_cans[can] = true;
        }
    }

    auto& num_cover = index.indepInfo->num_cover_;
    static vector<ui> idx(num_cover);
    static vector<pair<ui, ui>> up_down_idxs(num_cover-1);  // up_idx & down_idx of matched edges
    static ui depth;
    static vector<vector<ui>> edges_idxs(num_cover-1);
    depth = 0;
    ui edge_start, edge_end;
    if (subInfo[depth].connected) {
        auto& edge_up_start = subInfo[depth].subs.edge_up_offset[index.subInfo_[depth].subs.cur_s-1];
        auto& edge_up_end = subInfo[depth].subs.edge_up_offset[index.subInfo_[depth].subs.cur_s];
        edge_start = subInfo[depth].subs.edge_down_offset[edge_up_start];
        edge_end = subInfo[depth].subs.edge_down_offset[edge_up_end];
    } else {
        edge_start = subInfo[depth].subs.edge_down_offset[index.subInfo_[depth].subs.cur_s-1];
        edge_end = subInfo[depth].subs.edge_down_offset[index.subInfo_[depth].subs.cur_s];
    }
    edges_idxs[depth].clear();
    for (ui i = edge_start; i < edge_end; i++) {
        edges_idxs[depth].emplace_back(index.subInfo_[depth].subs.edge_down_idxs[i]);
    }
    idx[depth] = 0;
    static ui con_num;  // record the conflicts number
    con_num = 0;
    while(true) {
        while(idx[depth] < edges_idxs[depth].size()) {
            ui up_idx, down_idx;
            auto& edge_idx =  edges_idxs[depth][idx[depth]];
            idx[depth]++;
            if (index.subInfo_[depth].connected) {
                up_idx = index.subInfo_[depth].up_idxs[edge_idx];
                down_idx = index.subInfo_[depth].down_idxs[edge_idx];
            } else {
                auto down_num = index.subInfo_[depth].down_cans.size();
                up_idx = edge_idx/down_num;
                down_idx = edge_idx%down_num;
            }
            up_down_idxs[depth] = make_pair(up_idx, down_idx);
            auto& up = index.subInfo_[depth].up_cans[up_idx];
            auto& down = index.subInfo_[depth].down_cans[down_idx];
            if (depth == 0) {
                if (up == down) continue;
                cans[order[depth]][0] = up;
                if (used_cans[up]) con_num++;
                visited_v[up] = true;
            }
            if (visited_v[down]) continue;
            visited_v[down] = true;
            if (used_cans[down]) con_num++;
            cans[order[depth+1]][0] = down;
            if (depth >= num_cover - 2) {
                if (con_num == 0 && uncon == true) {  // no conflicts & searched the no-conflicts case
                    mpz_add(embedding_total, embedding_total, embedding_uncon);
                } else {
#ifdef HOMOMORPHISM
                    index.indepInfo->homoEnum();
#else
                    index.indepInfo->enumeration(index.q_graph_);
#endif
                    if (con_num == 0) {  // no conflicts, record the results
                        mpz_set(embedding_uncon, embedding_step);
                        uncon = true;
                    }
                    mpz_add(embedding_total, embedding_total, embedding_step);
                }
                if (depth == 0) {
                    if (used_cans[up]) con_num--;
                    visited_v[up] = false;
                }
                if (used_cans[down]) con_num--;
                visited_v[down] = false;
            } else {
                depth++;
                // compute the valid_edges of nxt depth, based on down_idx
                if (subInfo[depth].connected) {
                    ui nxt_idx = subInfo[depth-1].nxt_up_idxs[down_idx];
                    auto& edge_up_start = subInfo[depth].subs.edge_up_offset[index.subInfo_[depth].subs.cur_s-1];
                    auto& edge_up_end = subInfo[depth].subs.edge_up_offset[index.subInfo_[depth].subs.cur_s];
                    ui edge_up_idx;
                    for (edge_up_idx = edge_up_start; edge_up_idx < edge_up_end; edge_up_idx++) {
                        if (subInfo[depth].subs.edge_up_idxs[edge_up_idx] == nxt_idx) break;
                    }
                    if (edge_up_idx == edge_up_end) {  // if nxt_idx is invalid in nxtSub
                        break;
                    }
                    edge_start = subInfo[depth].subs.edge_down_offset[edge_up_idx];
                    edge_end = subInfo[depth].subs.edge_down_offset[edge_up_idx+1];
                    edges_idxs[depth].clear();
                    for (ui i = edge_start; i < edge_end; i++) {
                        edges_idxs[depth].emplace_back(index.subInfo_[depth].subs.edge_down_idxs[i]);
                    }
                } else {
                    ui nxt_idx = subInfo[depth-1].nxt_up_idxs[down_idx];
                    auto down_num = index.subInfo_[depth].down_cans.size();
                    edge_start = subInfo[depth].subs.edge_down_offset[index.subInfo_[depth].subs.cur_s-1];
                    edge_end = subInfo[depth].subs.edge_down_offset[index.subInfo_[depth].subs.cur_s];
                    edges_idxs[depth].clear();
                    for (ui i = edge_start; i < edge_end; i++) {
                        auto& edge_idx = subInfo[depth].subs.edge_down_idxs[i];
                        if (edge_idx/down_num == nxt_idx)
                            edges_idxs[depth].emplace_back(index.subInfo_[depth].subs.edge_down_idxs[i]);
                    }
                }
                idx[depth] = 0;
            }
        }
        depth--;
        if (depth == (ui)-1) break;
        ui up_idx, down_idx;
        auto& edge_idx =  edges_idxs[depth][idx[depth]-1];
        if (index.subInfo_[depth].connected) {
            up_idx = index.subInfo_[depth].up_idxs[edge_idx];
            down_idx = index.subInfo_[depth].down_idxs[edge_idx];
        } else {
            auto down_num = index.subInfo_[depth].down_cans.size();
            up_idx = edge_idx/down_num;
            down_idx = edge_idx%down_num;
        }
        up_down_idxs[depth] = make_pair(up_idx, down_idx);
        auto& up = index.subInfo_[depth].up_cans[up_idx];
        auto& down = index.subInfo_[depth].down_cans[down_idx];
        if (depth == 0) {
            if (used_cans[up]) con_num--;
            visited_v[up] = false;
        }
        if (used_cans[down]) con_num--;
        visited_v[down] = false;
    }

#ifdef ANALYZE_TIME
    FiPEIndex::enumerate_time += TimeOp::getClockNan() - start_time;
#endif
    return;
}

/**
 * compute substitutable information for up & down cans
 */
void
EvaluateQuery::comSub(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& up_cans = subInfo.up_cans;
    auto& subs = subInfo.subs;
    subs.cur_s = 0;
    auto& up_indep = subInfo.nbrs.up_indep_;
    auto& down_indep = subInfo.nbrs.down_indep_;
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;

    // compute sub by using 2 offset alternately
    // c_* store the complete sub info, p_* store partial sub info
    auto& c_cnt = subs.c_cnt;
    auto& c_offset = subs.edge_up_offset;
    auto& c_idxs = subs.edge_up_idxs;
    auto& p_cnt = subs.p_cnt;
    auto& p_offset = subs.p_edge_up_offset;
    auto& p_idxs = subs.p_edge_up_idxs;
    bool same = true;

    // init up subs
    auto& up_group_num = subInfo.subs.up_group_num;
    c_offset.resize(up_cans.size()+1);
    p_offset.resize(up_cans.size()+1);
    c_idxs.resize(up_cans.size());
    p_idxs.resize(up_cans.size());
    up_group_num.clear();
    up_group_num.resize(up_cans.size(), 0);  // 0->no group
    c_cnt = 1;
    c_offset[0] = 0; c_offset[1] = up_cans.size();
    for (ui i = 0; i < up_cans.size(); i++) c_idxs[i] = i;

    // up_indep, just separate up_cans(idxs), only work with start vertex
    for (auto& unbr : up_indep) {
        vector<bool> up_grouped(c_idxs.size(), false);
        p_cnt = 0;
        for (ui i = 0; i < c_cnt; i++) {
            auto start = c_offset[i], end = c_offset[i+1];
            for (auto idx1 = start; idx1 < end; idx1++) {
                auto& cans_idx1 = c_idxs[idx1];
                if (up_grouped[cans_idx1]) continue;
                p_cnt++;
                up_grouped[cans_idx1] = true;
                up_group_num[cans_idx1] = p_cnt;
                p_offset[p_cnt] = p_offset[p_cnt-1];
                p_idxs[p_offset[p_cnt]++] = cans_idx1;
                auto& cans1 = inf_cans[unbr][cans_idx1];
                for (auto idx2 = idx1 + 1; idx2 < end; idx2++) {
                    auto& cans_idx2 = c_idxs[idx2];
                    auto& cans2 = inf_cans[unbr][cans_idx2];
                    if (up_grouped[cans_idx2]) continue;
                    if (cans1.size() != cans2.size()) continue;
                    same = true;
                    for (ui com_idx = 0; com_idx < cans1.size(); com_idx++) {
                        if (cans1[com_idx] != cans2[com_idx]) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        p_idxs[p_offset[p_cnt]++] = cans_idx2;
                        up_grouped[cans_idx2] = true;
                        up_group_num[cans_idx2] = p_cnt;
                    }
                }
            }
        }
        c_idxs.swap(p_idxs);
        c_offset.swap(p_offset);
        c_cnt = p_cnt;
    }

    // down_indep, seperate down_cans(idxs)
    auto& down_group_num = subInfo.subs.down_group_num;
    auto& down_cans = subInfo.down_cans;
    c_idxs.resize(down_cans.size());
    p_idxs.resize(down_cans.size());
    c_offset.resize(down_cans.size()+1);
    p_offset.resize(down_cans.size()+1);
    down_group_num.clear();
    down_group_num.resize(down_cans.size(), 0);  // 0->no group
    for (ui i = 0; i < down_cans.size(); i ++) c_idxs[i] = i;
    c_cnt = 1;
    c_offset[0] = 0; c_offset[1] = down_cans.size();
    for (auto& unbr : down_indep) {
        vector<bool> down_grouped(c_idxs.size(), false);
        p_cnt = 0;
        for (ui i = 0; i < c_cnt; i++) {
            auto start = c_offset[i], end = c_offset[i+1];
            // 1. put all vertices that have no influence on unbr to one group
            p_cnt++;
            p_offset[p_cnt] = p_offset[p_cnt-1];
            for (auto idx1 = start; idx1 < end; idx1++) {
                auto& cans_idx = c_idxs[idx1];
                if (!influenced[unbr][cans_idx]) {
                    p_idxs[p_offset[p_cnt]++] = cans_idx;
                    down_grouped[cans_idx] = true;
                    down_group_num[cans_idx] = p_cnt;
                }
            }
            if (p_offset[p_cnt] == p_offset[p_cnt-1]) p_cnt--;
            for (auto idx1 = start; idx1 < end; idx1++) {
                auto& cans_idx1 = c_idxs[idx1];
                if (down_grouped[cans_idx1]) continue;
                p_cnt++;
                p_offset[p_cnt] = p_offset[p_cnt-1];
                p_idxs[p_offset[p_cnt]++] = cans_idx1;
                auto& cans1 = inf_cans[unbr][cans_idx1];
                down_grouped[cans_idx1] = true;
                down_group_num[cans_idx1] = p_cnt;
                for (auto idx2 = idx1 + 1; idx2 < end; idx2++) {
                    auto& cans_idx2 = c_idxs[idx2];
                    auto& cans2 = inf_cans[unbr][cans_idx2];
                    if (down_grouped[cans_idx2]) continue;
                    if (cans1.size() != cans2.size()) continue;
                    same = true;
                    for (ui com_idx = 0; com_idx < cans1.size(); com_idx++) {
                        if (cans1[com_idx] != cans2[com_idx]) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        p_idxs[p_offset[p_cnt]++] = cans_idx2;
                        down_grouped[cans_idx2] = true;
                        down_group_num[cans_idx2] = p_cnt;
                    }
                }
            }
        }
        c_idxs.swap(p_idxs);
        c_offset.swap(p_offset);
        c_cnt = p_cnt;
    }
    // after up_indep & down_indep info, compute edge info
    if (subInfo.connected) {
        comEdgeSubCon(index, depth);
    } else {
        comEdgeSubDis(index, depth);
    }
    return;
}

void
EvaluateQuery::comEdgeSubCon(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& subs = subInfo.subs;
    auto& up_cans = subInfo.up_cans;
    auto& up2down = subInfo.up2down;     // down_cans of each up_can
    auto& shared = subInfo.nbrs.shared_;
    auto& grouped = subInfo.grouped;
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;
    auto& down_idxs = subInfo.down_idxs;
    auto& up_group_num = subInfo.subs.up_group_num;
    auto& down_group_num = subInfo.subs.down_group_num;

    // compute sub by use 2 offset alternately
    // c_* store the complete sub info, p_* store partial sub info
    auto& c_cnt = subs.c_cnt;
    auto& c_edge_up_offset = subs.edge_up_offset;
    auto& c_edge_up_idxs = subs.edge_up_idxs;
    auto& c_edge_down_offset = subs.edge_down_offset;
    auto& c_edge_down_idxs = subs.edge_down_idxs;
    auto& p_cnt = subs.p_cnt;
    auto& p_edge_up_offset = subs.p_edge_up_offset;
    auto& p_edge_up_idxs = subs.p_edge_up_idxs;
    auto& p_edge_down_offset = subs.p_edge_down_offset;
    auto& p_edge_down_idxs = subs.p_edge_down_idxs;
    ui max_size = up2down.size() > up_cans.size() ? up2down.size() : up_cans.size();

    // init edge_idxs
    c_cnt = 1;
    c_edge_up_offset.resize(max_size+2);  // one spare space for invalid edges
    p_edge_up_offset.resize(max_size+2);
    // c_edge_down_offset is initialized during edge building in comCurSpace
    c_edge_down_offset.resize(max_size+2);
    p_edge_down_offset.resize(max_size+2);
    p_edge_up_idxs.resize(max_size);
    c_edge_up_idxs.resize(max_size);
    c_edge_down_idxs.resize(up2down.size());
    p_edge_down_idxs.resize(up2down.size());
    c_edge_up_offset[0] = 0; c_edge_up_offset[1] = up_cans.size();
    for (ui i = 0; i < up_cans.size(); i++) c_edge_up_idxs[i] = i;
    for (ui i = 0; i < up2down.size(); i++) c_edge_down_idxs[i] = i;
    grouped.resize(up2down.size());
    bool same;

    for (auto& unbr : shared) {
        fill(grouped.begin(), grouped.end(), false);
        p_cnt = 0;
        for (ui group_idx = 0; group_idx < c_cnt; group_idx++) {
            // can not put all vertices that have no influence on unbr to one group
            // because down_group_num maybe different
            auto& up_start = c_edge_up_offset[group_idx];
            auto& up_end = c_edge_up_offset[group_idx+1];
            for (auto up_idx_idx1 = up_start; up_idx_idx1 < up_end; up_idx_idx1++) {
                auto& up_idx1 = c_edge_up_idxs[up_idx_idx1];
                auto& down_start1 = c_edge_down_offset[up_idx_idx1];
                auto& down_end1 = c_edge_down_offset[up_idx_idx1+1];
                for (auto down_idx_idx1 = down_start1; down_idx_idx1 < down_end1; down_idx_idx1++) {
                    auto& edge_idx1 = c_edge_down_idxs[down_idx_idx1];
                    if (grouped[edge_idx1]) continue;
                    auto& down_idx1 = down_idxs[edge_idx1];
                    auto& cans1 = inf_cans[unbr][edge_idx1];
                    p_cnt++;
                    p_edge_up_offset[p_cnt] = p_edge_up_offset[p_cnt-1];
                    p_edge_up_idxs[p_edge_up_offset[p_cnt]++] = up_idx1;
                    p_edge_down_offset[p_edge_up_offset[p_cnt]] = p_edge_down_offset[p_edge_up_offset[p_cnt] - 1];
                    p_edge_down_idxs[p_edge_down_offset[p_edge_up_offset[p_cnt]]++] = edge_idx1;
                    grouped[edge_idx1] = true;
                    for (ui up_idx_idx2 = up_idx_idx1; up_idx_idx2 < up_end; up_idx_idx2++) {
                        auto& up_idx2 = c_edge_up_idxs[up_idx_idx2];
                        auto& down_start2 = c_edge_down_offset[up_idx_idx2];
                        auto& down_end2 = c_edge_down_offset[up_idx_idx2+1];
                        if (up_idx2 != p_edge_up_idxs[p_edge_up_offset[p_cnt]-1]) {
                            p_edge_up_idxs[p_edge_up_offset[p_cnt]++] = up_idx2;
                            p_edge_down_offset[p_edge_up_offset[p_cnt]] = p_edge_down_offset[p_edge_up_offset[p_cnt] - 1];
                        }
                        for (auto down_idx_idx2 = down_start2; down_idx_idx2 < down_end2; down_idx_idx2++) {
                            auto& edge_idx2 = c_edge_down_idxs[down_idx_idx2];
                            auto& down_idx2 = down_idxs[edge_idx2];
                            if (grouped[edge_idx2]
                                || down_group_num[down_idx1] != down_group_num[down_idx2]
                                || up_group_num[up_idx1] != up_group_num[up_idx2]) continue;
                            same = true;
                            if (influenced[unbr][edge_idx2] || influenced[unbr][edge_idx1]) {
                                auto& cans2 = inf_cans[unbr][edge_idx2];
                                if (cans1.size() != cans2.size()) continue;
                                for (ui com_idx = 0; com_idx < cans1.size(); com_idx++) {
                                    if (cans1[com_idx] != cans2[com_idx]) {
                                        same = false;
                                        break;
                                    }
                                }
                            }
                            if (same) {
                                p_edge_down_idxs[p_edge_down_offset[p_edge_up_offset[p_cnt]]++] = edge_idx2;
                                grouped[edge_idx2] = true;
                            }
                        }
                        // if no edge is added for up_idx, edge_up_offset should not store this up_idx
                        if (p_edge_down_offset[p_edge_up_offset[p_cnt]] == p_edge_down_offset[p_edge_up_offset[p_cnt] - 1])
                            p_edge_up_offset[p_cnt]--;
                    }
                }
            }
        }
        c_edge_up_idxs.swap(p_edge_up_idxs);
        c_edge_up_offset.swap(p_edge_up_offset);
        c_edge_down_idxs.swap(p_edge_down_idxs);
        c_edge_down_offset.swap(p_edge_down_offset);
        c_cnt = p_cnt;
    }

    if (shared.empty()) {
        fill(grouped.begin(), grouped.end(), false);
        p_cnt = 0;
        for (ui group_idx = 0; group_idx < c_cnt; group_idx++) {
            auto& up_start = c_edge_up_offset[group_idx];
            auto& up_end = c_edge_up_offset[group_idx+1];
            for (auto up_idx_idx1 = up_start; up_idx_idx1 < up_end; up_idx_idx1++) {
                auto& up_idx1 = c_edge_up_idxs[up_idx_idx1];
                auto& down_start1 = c_edge_down_offset[up_idx_idx1];
                auto& down_end1 = c_edge_down_offset[up_idx_idx1+1];
                for (auto down_idx_idx1 = down_start1; down_idx_idx1 < down_end1; down_idx_idx1++) {
                    auto& edge_idx1 = c_edge_down_idxs[down_idx_idx1];
                    if (grouped[edge_idx1]) continue;
                    auto& down_idx1 = down_idxs[edge_idx1];
                    p_cnt++;
                    p_edge_up_offset[p_cnt] = p_edge_up_offset[p_cnt-1];
                    p_edge_up_idxs[p_edge_up_offset[p_cnt]++] = up_idx1;
                    p_edge_down_offset[p_edge_up_offset[p_cnt]] = p_edge_down_offset[p_edge_up_offset[p_cnt] - 1];
                    p_edge_down_idxs[p_edge_down_offset[p_edge_up_offset[p_cnt]]++] = edge_idx1;
                    grouped[edge_idx1] = true;
                    for (ui up_idx_idx2 = up_idx_idx1; up_idx_idx2 < up_end; up_idx_idx2++) {
                        auto& up_idx2 = c_edge_up_idxs[up_idx_idx2];
                        auto& down_start2 = c_edge_down_offset[up_idx_idx2];
                        auto& down_end2 = c_edge_down_offset[up_idx_idx2+1];
                        if (up_idx2 != p_edge_up_idxs[p_edge_up_offset[p_cnt]-1]) {
                            p_edge_up_idxs[p_edge_up_offset[p_cnt]++] = up_idx2;
                            p_edge_down_offset[p_edge_up_offset[p_cnt]] = p_edge_down_offset[p_edge_up_offset[p_cnt] - 1];
                        }
                        for (auto down_idx_idx2 = down_start2; down_idx_idx2 < down_end2; down_idx_idx2++) {
                            auto& edge_idx2 = c_edge_down_idxs[down_idx_idx2];
                            auto& down_idx2 = down_idxs[edge_idx2];
                            if (grouped[edge_idx2]
                                || down_group_num[down_idx1] != down_group_num[down_idx2]
                                || up_group_num[up_idx1] != up_group_num[up_idx2]) continue;
                                p_edge_down_idxs[p_edge_down_offset[p_edge_up_offset[p_cnt]]++] = edge_idx2;
                                grouped[edge_idx2] = true;
                        }
                        // if no edge is added for up_idx, edge_up_offset should not store this up_idx
                        if (p_edge_down_offset[p_edge_up_offset[p_cnt]] == p_edge_down_offset[p_edge_up_offset[p_cnt] - 1])
                            p_edge_up_offset[p_cnt]--;
                    }
                }
            }
        }
        c_edge_up_idxs.swap(p_edge_up_idxs);
        c_edge_up_offset.swap(p_edge_up_offset);
        c_edge_down_idxs.swap(p_edge_down_idxs);
        c_edge_down_offset.swap(p_edge_down_offset);
        c_cnt = p_cnt;
    }
#ifdef ANALYZE_FUNC_MEMORY
    mem::printVmRSS("Edge_Equ_Con");
#endif
    return;
}

void
EvaluateQuery::comEdgeSubDis(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& up_cans = subInfo.up_cans;
    auto& down_cans = subInfo.down_cans;
    auto& subs = subInfo.subs;
    auto& shared = subInfo.nbrs.shared_;
    auto& grouped = subInfo.grouped;
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;
    auto& edge_valid = subInfo.edge_valid;
    auto& up_group_num = subInfo.subs.up_group_num;
    auto& down_group_num = subInfo.subs.down_group_num;

    // compute sub by use 2 offset alternately
    // c_* store the complete sub info, p_* store partial sub info
    auto& c_cnt = subs.c_cnt;
    auto& c_edge_up_offset = subs.edge_up_offset;
    auto& c_edge_up_idxs = subs.edge_up_idxs;
    auto& c_edge_down_offset = subs.edge_down_offset;
    auto& c_edge_down_idxs = subs.edge_down_idxs;
    auto& p_cnt = subs.p_cnt;
    auto& p_edge_down_offset = subs.p_edge_down_offset;
    auto& p_edge_down_idxs = subs.p_edge_down_idxs;

    // init edge_idxs
    auto up_num = up_cans.size();
    auto down_num = down_cans.size();
    auto edge_num = up_num*down_num;
    c_edge_down_offset.resize(edge_num+1);
    p_edge_down_offset.resize(edge_num+1);
    c_edge_down_idxs.resize(edge_num);
    p_edge_down_idxs.resize(edge_num);
    c_edge_down_offset[0] = 0, c_edge_down_offset[1] = edge_num;
    for (ui i = 0; i < edge_num; i++) c_edge_down_idxs[i] = i;
    grouped.resize(edge_num);
    bool same;

    // scanning edge, up_idx = edge_idx/down_cans.size(), down_idx = edge_idx%down_cans.size()
    //               up = up_cans[up_idx], down = down_cans[down_idx]
    for (auto& unbr : shared) {
        p_cnt = 0;
        fill(grouped.begin(), grouped.end(), false);
        for (ui group_id = 0; group_id < c_cnt; group_id++) {
            auto& edge_start = c_edge_down_offset[group_id];
            auto& edge_end = c_edge_down_offset[group_id+1];
            for (ui edge_idx_idx1 = edge_start; edge_idx_idx1 < edge_end; edge_idx_idx1++) {
                auto& edge_idx1 = c_edge_down_idxs[edge_idx_idx1];
                if (!edge_valid[edge_idx1] || grouped[edge_idx1]) continue;
                auto up_idx1 = edge_idx1/down_num;
                auto down_idx1 = edge_idx1%down_num;
                p_cnt+=1;
                p_edge_down_offset[p_cnt] = p_edge_down_offset[p_cnt - 1];
                p_edge_down_idxs[p_edge_down_offset[p_cnt]++] = edge_idx1;
                auto& cans1 = inf_cans[unbr][edge_idx1];
                grouped[edge_idx1] = true;
                for (ui edge_idx_idx2 = edge_idx_idx1 + 1; edge_idx_idx2 < edge_end; edge_idx_idx2++) {
                    auto& edge_idx2 = c_edge_down_idxs[edge_idx_idx2];
                    auto up_idx2 = edge_idx2/down_num;
                    auto down_idx2 = edge_idx2%down_num;
                    if (!edge_valid[edge_idx2] || grouped[edge_idx2]
                        || up_group_num[up_idx1] != up_group_num[up_idx2]
                        || down_group_num[down_idx1] != down_group_num[down_idx2]) continue;
                    same = true;
                    if (influenced[unbr][edge_idx2] || influenced[unbr][edge_idx1]) {
                        auto& cans2 = inf_cans[unbr][edge_idx2];
                        if (cans1.size() != cans2.size()) continue;
                        for (ui com_idx = 0; com_idx < cans1.size(); com_idx++) {
                            if (cans1[com_idx] != cans2[com_idx]) {
                                same = false;
                                break;
                            }
                        }
                    }
                    if (same) {
                        p_edge_down_idxs[p_edge_down_offset[p_cnt]++] = edge_idx2;
                        grouped[edge_idx2] = true;
                    }
                }
            }
        }
        c_edge_down_idxs.swap(p_edge_down_idxs);
        c_edge_down_offset.swap(p_edge_down_offset);
        c_cnt = p_cnt;
    }

    if (shared.empty()) {
        p_cnt = 0;
        fill(grouped.begin(), grouped.end(), false);
        for (ui group_id = 0; group_id < c_cnt; group_id++) {
            auto& edge_start = c_edge_down_offset[group_id];
            auto& edge_end = c_edge_down_offset[group_id+1];
            for (ui edge_idx_idx1 = edge_start; edge_idx_idx1 < edge_end; edge_idx_idx1++) {
                auto& edge_idx1 = c_edge_down_idxs[edge_idx_idx1];
                if (!edge_valid[edge_idx1] || grouped[edge_idx1]) continue;
                auto up_idx1 = edge_idx1/down_num;
                auto down_idx1 = edge_idx1%down_num;
                p_cnt+=1;
                p_edge_down_offset[p_cnt] = p_edge_down_offset[p_cnt - 1];
                p_edge_down_idxs[p_edge_down_offset[p_cnt]++] = edge_idx1;
                grouped[edge_idx1] = true;
                for (ui edge_idx_idx2 = edge_idx_idx1 + 1; edge_idx_idx2 < edge_end; edge_idx_idx2++) {
                    auto& edge_idx2 = c_edge_down_idxs[edge_idx_idx2];
                    auto up_idx2 = edge_idx2/down_num;
                    auto down_idx2 = edge_idx2%down_num;
                    if (!edge_valid[edge_idx2] || grouped[edge_idx2]
                        || up_group_num[up_idx1] != up_group_num[up_idx2]
                        || down_group_num[down_idx1] != down_group_num[down_idx2]) continue;
                        p_edge_down_idxs[p_edge_down_offset[p_cnt]++] = edge_idx2;
                        grouped[edge_idx2] = true;
                }
            }
        }
        c_edge_down_idxs.swap(p_edge_down_idxs);
        c_edge_down_offset.swap(p_edge_down_offset);
        c_cnt = p_cnt;
    }
#ifdef ANALYZE_FUNC_MEMORY
    mem::printVmRSS("Edge_Equ_Dis");
#endif
    return;
}

/**
 * compute valid_cans of all unbrs for start_vertex
 */
bool
EvaluateQuery::comStartCans(FiPEIndex& index) {
    auto u = index.order_[0];
    auto& subInfo = index.subInfo_[0];
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;
    auto& up_cans = subInfo.up_cans;
    auto& up_indep = subInfo.nbrs.up_indep_;
    auto& cans = index.valid_cans_[u].back();
    auto can_cnt = index.valid_cans_[u].back().size();
    for (auto& unbr : up_indep) {
        influenced[unbr].clear();
        inf_cans[unbr].clear();
        influenced[unbr].reserve(can_cnt);
        inf_cans[unbr].reserve(can_cnt);
    }
    up_cans.clear();
    up_cans.reserve(can_cnt);

    // compute the valid_cans of all nbrs for each cans
    ui unbrs_cnt;
    auto unbrs = index.q_graph_->getVertexNeighbors(u, unbrs_cnt);
    for (ui v_idx = 0; v_idx < can_cnt; v_idx++) {
        auto& v = cans[v_idx];
        bool valid = true;
        ui edge_idx = v_idx;
        if (index.subCans_[0].splitted) edge_idx += index.subCans_[0].cur_start;
        for (ui i = 0; i < unbrs_cnt; i++) {
            auto& unbr = unbrs[i];
            auto& edges = *(index.index_[u][unbr].back());
            auto vnbrs_cnt = edges.offset_[edge_idx + 1] - edges.offset_[edge_idx];
            if (vnbrs_cnt == 0) {
                valid = false;
                break;
            }
        }
        if (valid) {
            up_cans.emplace_back(cans[v_idx]);
            for (auto& unbr : up_indep) {
                auto &edges = *(index.index_[u][unbr].back());
                auto vnbrs_cnt = edges.offset_[edge_idx + 1] - edges.offset_[edge_idx];
                auto vnbrs = edges.edge_ + edges.offset_[edge_idx];
                influenced[unbr].emplace_back(true);  // can be removed for start_vertex
                inf_cans[unbr].emplace_back(vnbrs, vnbrs+vnbrs_cnt);
            }
        }
    }
    return up_cans.size();  // 0->false, o.w.->true
}

/**
 * compute valid_cans for down_indep_ & shared_
 */
bool
EvaluateQuery::comCurSpace(FiPEIndex& index, ui depth) {
    auto& up = index.order_[depth];
    auto& down = index.order_[depth+1];
    auto& subInfo = index.subInfo_[depth];
    auto& up_cans = subInfo.up_cans;
    auto& subs = subInfo.subs;
    auto& edge_down_offset = subs.edge_down_offset;  // valid_cans range of each cans to nxt
    auto& up2down = subInfo.up2down;       // valid_cans of nxt
    auto& down_record = subInfo.subs.down_record;
    bool success = true;

    // 1.compute the valid_cans of shared for up
    auto& up_shared_valid_cans = subInfo.up_shared_valid_cans;      // cans of [unbr][up_idx]
    auto& shared = subInfo.nbrs.shared_;
    ui up_valid_idx = 0;
    for (auto& nbr : shared) {
        up_shared_valid_cans[nbr].resize(up_cans.size());
    }
    for (ui v_idx = 0; v_idx < up_cans.size(); v_idx++, up_valid_idx++) {
        up_cans[up_valid_idx] = up_cans[v_idx];
        for (auto& unbr : shared) {
            auto vnbrs = move(index.getNeighbors(up, unbr, up_cans[v_idx]));
            if (vnbrs.size() == 0) {
                up_valid_idx--;
                break;
            }
            up_shared_valid_cans[unbr][up_valid_idx].swap(vnbrs);
        }
    }
    if (up_valid_idx == 0) return false;
    up_cans.resize(up_valid_idx);

    // build edges
    if (subInfo.connected) {
        edge_down_offset.clear();
        edge_down_offset.emplace_back(0);
        up2down.clear();
        for (auto& up_can : up_cans) {
            auto vnbrs = move(index.getNeighbors(up, down, up_can));
            edge_down_offset.emplace_back(edge_down_offset.back()+vnbrs.size());
            up2down.insert(up2down.end(), move(vnbrs.begin()), move(vnbrs.end()));
        }
    }

    // compute the down_cans
    auto& down_cans = subInfo.down_cans;
    if (subInfo.connected) {
        down_cans.clear();
        auto& down_idxs = subInfo.down_idxs;
        down_idxs.resize(up2down.size());
        for (ui i = 0; i < up2down.size(); i++) {
            if (down_record[up2down[i]] == (ui)-1) {
                down_record[up2down[i]] = down_cans.size();
                down_cans.emplace_back(up2down[i]);
            }
            down_idxs[i] = down_record[up2down[i]];
        }
    } else {
        down_cans = index.valid_cans_[down].back();
    }

    // compute valid_cans for down_indep & down_shared_valid_cans & delayed_nbrs
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;
    auto& down_indep = subInfo.nbrs.down_indep_;
    auto& delayed_nbrs = subInfo.nbrs.delayed_;
    ui down_valid_idx = 0;
    auto& down_valid = subInfo.down_valid;
    auto& down_shared_valid_cans = subInfo.down_shared_valid_cans;
    down_valid.clear();
    down_valid.resize(down_cans.size(), true);
    for (auto& nbr : down_indep) {
        influenced[nbr].clear();
        inf_cans[nbr].clear();
        influenced[nbr].resize(down_cans.size());
        inf_cans[nbr].resize(down_cans.size());
    }
    for (auto& nbr : shared) {
        down_shared_valid_cans[nbr].resize(down_cans.size());
    }
    for (auto& nbr : delayed_nbrs) {
        influenced[nbr].clear();
        inf_cans[nbr].clear();
        influenced[nbr].resize(down_cans.size());
        inf_cans[nbr].resize(down_cans.size());
    }
    for (ui v_idx = 0; v_idx < down_cans.size(); v_idx++, down_valid_idx++) {
        auto& v = down_cans[v_idx];
        down_record[v] = down_valid_idx;
        for (auto& unbr : down_indep) {
            auto vnbrs = move(index.getNeighbors(down, unbr, v));
            if (vnbrs.size() == 0) {
                down_valid[v_idx] = false;
                down_valid_idx--;
                break;
            }
            if (index.valid_cans_[unbr].back().size() == vnbrs.size()) {
                influenced[unbr][down_valid_idx] = false;
                continue;  // means no changes
            }

            // write to down_valid_idx, down_cans will be shrinked at the end of this function
            influenced[unbr][down_valid_idx] = true;
            inf_cans[unbr][down_valid_idx].swap(vnbrs);
        }
        if (!down_valid[v_idx]) continue;
        for (auto& unbr : shared) {
            auto vnbrs = move(index.getNeighbors(down, unbr, v));
            if (vnbrs.size() == 0) {
                down_valid_idx--;
                down_valid[v_idx] = false;
                break;
            }
            down_shared_valid_cans[unbr][down_valid_idx].swap(vnbrs);
        }
        if (!down_valid[v_idx]) continue;
        for (auto& unbr : delayed_nbrs) {
            auto vnbrs = move(index.getNeighbors(down, unbr, v));
            if (vnbrs.size() == 0) {
                down_valid[v_idx] = false;
                down_valid_idx--;
                break;
            }
            if (index.valid_cans_[unbr].back().size() == vnbrs.size()) {
                influenced[unbr][down_valid_idx] = false;
                continue;  // means no changes
            }

            influenced[unbr][down_valid_idx] = true;
            inf_cans[unbr][down_valid_idx].swap(vnbrs);
        }
    }
    if (down_valid_idx != 0) {
        // compute the valid_cans of shared for edges, based on down(up)_shared_valid_cans
        if (subInfo.connected) {
            success = comSharedCon(index, depth);
        } else {
            success = comSharedDis(index, depth);
        }
    } else {
        success = false;
    }

    // recover down_record
    ui valid_idx = 0;
    for (ui v_idx = 0; v_idx < down_cans.size(); v_idx++) {
        auto& down_can = down_cans[v_idx];
        down_record[down_can] = (ui)-1;
        if (down_valid[v_idx]) down_cans[valid_idx++] = down_can;
    }
    down_cans.resize(valid_idx);
    return success;
}

inline bool
EvaluateQuery::comSharedCon(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& up_cans = subInfo.up_cans;
    auto& subs = subInfo.subs;
    auto& edge_down_offset = subs.edge_down_offset;  // valid_cans range of each cans to nxt
    auto& up2down = subInfo.up2down;                // valid_cans of nxt
    // set group[idx] to true to jump the grouping of invalid edges
    auto& shared = subInfo.nbrs.shared_;
    auto& edge_valid = subInfo.edge_valid;
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;
    edge_valid.clear();
    edge_valid.resize(up2down.size(), true);
    auto& down_valid = subInfo.down_valid;
    auto& down_record = subInfo.subs.down_record;  // record v_idx to valid_down_cans
    auto& up_shared_valid_cans = subInfo.up_shared_valid_cans;      // cans of [unbr][up_idx]
    auto& down_shared_valid_cans = subInfo.down_shared_valid_cans;  // cans of [unbr][down_idx]
    auto& down_idxs = subInfo.down_idxs;
    auto& up_idxs = subInfo.up_idxs;
    up_idxs.resize(down_idxs.size());

    // reserve spaces for influenced
    for (auto& nbr : shared) {
        influenced[nbr].clear();
        inf_cans[nbr].clear();
        influenced[nbr].resize(up2down.size());
        inf_cans[nbr].resize(up2down.size());
    }

    // compute shared_valid_cans,
    // by the way, remove invalid down_cans & edges, update edge_down_offset, up2down, down_idxs
    ui edge_valid_idx = 0;
    auto valid_edge_down_offset = edge_down_offset;
    for (ui up_idx = 0; up_idx < up_cans.size(); up_idx++) {
        auto& edge_start = edge_down_offset[up_idx];
        auto& edge_end = edge_down_offset[up_idx+1];
        valid_edge_down_offset[up_idx+1] = valid_edge_down_offset[up_idx];
        for (ui edge_idx = edge_start; edge_idx < edge_end; edge_idx++) {
            auto& down = up2down[edge_idx];
            auto& valid_down_can_idx = down_record[down];
            auto& down_idx = down_idxs[edge_idx];
            if (!down_valid[down_idx]) continue;
            for (auto& nbr : shared) {
                auto vnbrs = move(SetOp::intersectTwo(up_shared_valid_cans[nbr][up_idx],
                                                      down_shared_valid_cans[nbr][valid_down_can_idx]));
                if (vnbrs.size() == 0) {
                    edge_valid[edge_idx] = false;
                    break;
                }
                if (vnbrs.size() == index.valid_cans_[nbr].back().size()) {
                    influenced[nbr][edge_valid_idx] = false;
                    continue;
                }
                influenced[nbr][edge_valid_idx] = true;
                inf_cans[nbr][edge_valid_idx].swap(vnbrs);
            }
            if (edge_valid[edge_idx]) {
                edge_valid_idx++;
                up2down[valid_edge_down_offset[up_idx+1]] = down;
                down_idxs[valid_edge_down_offset[up_idx+1]] = valid_down_can_idx;
                up_idxs[valid_edge_down_offset[up_idx+1]] = up_idx;
                valid_edge_down_offset[up_idx+1]++;
            }
        }
    }
    edge_down_offset.swap(valid_edge_down_offset);
    up2down.resize(edge_valid_idx);
    return up2down.size();
}

inline bool
EvaluateQuery::comSharedDis(FiPEIndex& index, ui depth) {
    auto& subInfo = index.subInfo_[depth];
    auto& up_cans = subInfo.up_cans;
    auto& down_cans = subInfo.down_cans;
    // set group[idx] to true to jump the grouping of invalid edges
    auto& shared = subInfo.nbrs.shared_;
    auto& edge_valid = subInfo.edge_valid;
    auto& influenced = subInfo.influenced;
    auto& inf_cans = subInfo.inf_cans;
    auto& down_valid = subInfo.down_valid;
    auto& up_valid_cans = subInfo.up_shared_valid_cans;      // cans of [unbr][up_idx]
    auto& down_valid_cans = subInfo.down_shared_valid_cans;  // cans of [unbr][down_idx]
    auto valid_down_cans = down_cans;
    ui valid_idx = 0;
    for (ui v_idx = 0; v_idx < down_cans.size(); v_idx++) {
        auto& down_can = down_cans[v_idx];
        if (down_valid[v_idx]) valid_down_cans[valid_idx++] = down_can;
    }
    valid_down_cans.resize(valid_idx);
    auto edge_num = up_cans.size()*valid_down_cans.size();
    edge_valid.clear();
    edge_valid.resize(edge_num, true);
    for (auto& nbr : shared) {
        influenced[nbr].clear();
        inf_cans[nbr].clear();
        influenced[nbr].resize(edge_num);
        inf_cans[nbr].resize(edge_num);
    }

    for (ui up_idx = 0; up_idx < up_cans.size(); up_idx++) {
        for (ui down_idx = 0; down_idx < valid_down_cans.size(); down_idx++) {
            auto edge_idx = up_idx*valid_down_cans.size() + down_idx;
            for (auto& nbr : shared) {
                auto vnbrs = move(SetOp::intersectTwo(up_valid_cans[nbr][up_idx], down_valid_cans[nbr][down_idx]));
                if (vnbrs.size() == 0) {
                    edge_valid[edge_idx] = false;
                    break;
                }
                if (vnbrs.size() == index.valid_cans_[nbr].back().size()) {
                    influenced[nbr][edge_idx] = false;
                    continue;
                }
                influenced[nbr][edge_idx] = true;
                inf_cans[nbr][edge_idx].swap(vnbrs);
            }
        }
    }
    return true;
}
