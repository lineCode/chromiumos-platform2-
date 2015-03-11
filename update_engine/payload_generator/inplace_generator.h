// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_INPLACE_GENERATOR_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_INPLACE_GENERATOR_H_

#include <set>
#include <string>
#include <vector>

#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/graph_types.h"

// InplaceGenerator contains all functionality related to the inplace algorithm
// for generating update payloads. These are the functions used when delta minor
// version is 1.

namespace chromeos_update_engine {

class InplaceGenerator {
 public:
  // Modifies blocks read by 'op' so that any blocks referred to by
  // 'remove_extents' are replaced with blocks from 'replace_extents'.
  // 'remove_extents' and 'replace_extents' must be the same number of blocks.
  // Blocks will be substituted in the order listed in the vectors.
  // E.g. if 'op' reads blocks 1, 2, 3, 4, 5, 6, 7, 8, remove_extents
  // contains blocks 6, 2, 3, 5, and replace blocks contains
  // 12, 13, 14, 15, then op will be changed to read from:
  // 1, 13, 14, 4, 15, 12, 7, 8
  static void SubstituteBlocks(Vertex* vertex,
                               const std::vector<Extent>& remove_extents,
                               const std::vector<Extent>& replace_extents);

  // Cuts 'edges' from 'graph' according to the AU algorithm. This means
  // for each edge A->B, remove the dependency that B occur before A.
  // Do this by creating a new operation X that copies from the blocks
  // specified by the edge's properties to temp space T. Modify B to read
  // from T rather than the blocks in the edge. Modify A to depend on X,
  // but not on B. Free space is found by looking in 'blocks'.
  // Returns true on success.
  static bool CutEdges(Graph* graph,
                       const std::set<Edge>& edges,
                       std::vector<CutEdgeVertexes>* out_cuts);

  // Creates all the edges for the graph. Writers of a block point to
  // readers of the same block. This is because for an edge A->B, B
  // must complete before A executes.
  static void CreateEdges(Graph* graph,
                          const std::vector<DeltaDiffGenerator::Block>& blocks);

  // Takes |op_indexes|, which is effectively a mapping from order in
  // which the op is performed -> graph vertex index, and produces the
  // reverse: a mapping from graph vertex index -> op_indexes index.
  static void GenerateReverseTopoOrderMap(
      const std::vector<Vertex::Index>& op_indexes,
      std::vector<std::vector<Vertex::Index>::size_type>* reverse_op_indexes);

  // Sorts the vector |cuts| by its |cuts[].old_dest| member. Order is
  // determined by the order of elements in op_indexes.
  static void SortCutsByTopoOrder(
      const std::vector<Vertex::Index>& op_indexes,
      std::vector<CutEdgeVertexes>* cuts);

  // Given a topologically sorted graph |op_indexes| and |graph|, alters
  // |op_indexes| to move all the full operations to the end of the vector.
  // Full operations should not be depended on, so this is safe.
  static void MoveFullOpsToBack(Graph* graph,
                                std::vector<Vertex::Index>* op_indexes);

  // Returns true iff there are no extents in the graph that refer to temp
  // blocks. Temp blocks are in the range [kTempBlockStart, kSparseHole).
  static bool NoTempBlocksRemain(const Graph& graph);

  // Takes a |graph|, which has edges that must be cut, as listed in
  // |cuts|.  Cuts the edges. Maintains a list in which the operations
  // will be performed (in |op_indexes|) and the reverse (in
  // |reverse_op_indexes|).  Cutting edges requires scratch space, and
  // if insufficient scratch is found, the file is reread and will be
  // send down (either as REPLACE or REPLACE_BZ).  Returns true on
  // success.
  static bool AssignTempBlocks(
      Graph* graph,
      const std::string& new_root,
      int data_fd,
      off_t* data_file_size,
      std::vector<Vertex::Index>* op_indexes,
      std::vector<std::vector<Vertex::Index>::size_type>* reverse_op_indexes,
      const std::vector<CutEdgeVertexes>& cuts);

  // Handles allocation of temp blocks to a cut edge by converting the
  // dest node to a full op. This removes the need for temp blocks, but
  // comes at the cost of a worse compression ratio.
  // For example, say we have A->B->A. It would first be cut to form:
  // A->B->N<-A, where N copies blocks to temp space. If there are no
  // temp blocks, this function can be called to convert it to the form:
  // A->B. Now, A is a full operation.
  static bool ConvertCutToFullOp(Graph* graph,
                                 const CutEdgeVertexes& cut,
                                 const std::string& new_root,
                                 int data_fd,
                                 off_t* data_file_size);

  // Takes a graph, which is not a DAG, which represents the files just
  // read from disk, and converts it into a DAG by breaking all cycles
  // and finding temp space to resolve broken edges.
  // The final order of the nodes is given in |final_order|
  // Some files may need to be reread from disk, thus |fd| and
  // |data_file_size| are be passed.
  // If |scratch_vertex| is not kInvalidIndex, removes it from
  // |final_order| before returning.
  // Returns true on success.
  static bool ConvertGraphToDag(Graph* graph,
                                const std::string& new_root,
                                int fd,
                                off_t* data_file_size,
                                std::vector<Vertex::Index>* final_order,
                                Vertex::Index scratch_vertex);

  // Creates a dummy REPLACE_BZ node in the given |vertex|. This can be used
  // to provide scratch space. The node writes |num_blocks| blocks starting at
  // |start_block|The node should be marked invalid before writing all nodes to
  // the output file.
  static void CreateScratchNode(uint64_t start_block,
                                uint64_t num_blocks,
                                Vertex* vertex);

  // The |blocks| vector contains a reader and writer for each block on the
  // filesystem that's being in-place updated. We populate the reader/writer
  // fields of |blocks| by calling this function.
  // For each block in |operation| that is read or written, find that block
  // in |blocks| and set the reader/writer field to the vertex passed.
  // |graph| is not strictly necessary, but useful for printing out
  // error messages.
  static bool AddInstallOpToBlocksVector(
      const DeltaArchiveManifest_InstallOperation& operation,
      const Graph& graph,
      Vertex::Index vertex,
      std::vector<DeltaDiffGenerator::Block>* blocks);

 private:
  // This should never be constructed.
  DISALLOW_IMPLICIT_CONSTRUCTORS(InplaceGenerator);
};

};  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_INPLACE_GENERATOR_H_