//
// Created by Jason Shen on 6/22/18.
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hierarchical_map_compiler/edge.h>
#include <hierarchical_map_compiler/graph.h>
#include <hierarchical_map_compiler/leaf_constraint_handler.h>
#include <hierarchical_map_compiler/map_cluster.h>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
extern "C" {
#include <sdd/sddapi.h>
}

std::string project_script_path;
namespace {
using std::pair;
using std::queue;
using std::set;
using std::unordered_map;
using std::vector;
pair<MapCluster *, vector<Edge *>> ConstructSimpleTestMapCluster() {
  MapCluster *test_cluster =
      new MapCluster(0, "0", {1, 2, 3}, nullptr, nullptr);
  vector<Edge *> edges = {new Edge("1", 1, 2), new Edge("2", 2, 3),
                          new Edge("3", 3, 5), new Edge("4", 1, 4),
                          new Edge("5", 1, 2)};
  test_cluster->SetInternalExternalEdgesFromEdgeList(edges);
  return {test_cluster, edges};
}
Vtree *ConstructBalancedVtree(
    const unordered_map<Edge *, SddLiteral> &edge_variable_map) {
  queue<Vtree *> vtree_pool;
  for (const auto &entry : edge_variable_map) {
    vtree_pool.push(new_leaf_vtree(entry.second));
  }
  while (vtree_pool.size() > 1) {
    Vtree *left_child = vtree_pool.front();
    vtree_pool.pop();
    Vtree *right_child = vtree_pool.front();
    vtree_pool.pop();
    Vtree *new_vtree_node = new_internal_vtree(left_child, right_child);
    vtree_pool.push(new_vtree_node);
  }
  assert(vtree_pool.size() == 1);
  Vtree *root_vtree = vtree_pool.front();
  set_vtree_properties(root_vtree);
  return root_vtree;
}
} // namespace
TEST(GRAPHILLION_LEAF_HANDLER_TEST, SIMPLE_TEST) {
  auto cluster_edge_pair = ConstructSimpleTestMapCluster();
  const auto &edges = cluster_edge_pair.second;
  unordered_map<Edge *, SddLiteral> edge_variable_map;
  unordered_map<SddLiteral, Edge *> variable_to_edge_map;
  unordered_map<MapCluster *, set<NodeSize>> terminal_nodes_per_cluster;
  terminal_nodes_per_cluster[cluster_edge_pair.first] = {1, 3};
  unordered_map<MapCluster *, set<pair<NodeSize, NodeSize>>>
      non_terminal_nodes_per_cluster;
  non_terminal_nodes_per_cluster[cluster_edge_pair.first] = {{1, 3}};
  SddLiteral psdd_literal_index = 10;
  for (Edge *cur_edge : edges) {
    edge_variable_map[cur_edge] = psdd_literal_index++;
    variable_to_edge_map[psdd_literal_index - 1] = cur_edge;
  }
  Vtree *local_vtree = ConstructBalancedVtree(edge_variable_map);
  unordered_map<MapCluster *, Vtree *> local_vtree_per_cluster;
  local_vtree_per_cluster[cluster_edge_pair.first] = local_vtree;
  LeafConstraintHandler *graphillion_handler =
      LeafConstraintHandler::GetGraphillionSddLeafConstraintHandler(
          &edge_variable_map, &variable_to_edge_map, &local_vtree_per_cluster,
          &terminal_nodes_per_cluster, &non_terminal_nodes_per_cluster,
          project_script_path + "/compile_graph.py",
          project_script_path + "/test", 2);
  const auto &non_terminal = graphillion_handler->non_terminal_path_constraint(
      cluster_edge_pair.first);
  const auto &terminal =
      graphillion_handler->terminal_path_constraint(cluster_edge_pair.first);
  EXPECT_TRUE(non_terminal.find({1, 3}) != non_terminal.end());
  EXPECT_TRUE(terminal.find(1) != terminal.end());
  EXPECT_TRUE(terminal.find(3) != terminal.end());
  for (Edge *cur_edge : edges) {
    delete (cur_edge);
  }
  delete (cluster_edge_pair.first);
  sdd_vtree_free(local_vtree);
}
