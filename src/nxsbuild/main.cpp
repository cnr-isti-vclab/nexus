#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#include "../core/mesh.h"
#include "../core/spatial_sort.h"
#include "../core/adjacency.h"
#include "../core/clustering.h"
#include "../core/clustering_metis.h"
#include "../core/micro_clustering.h"
#include "../core/micro_clustering_metis.h"
#include "../loaders/meshloader.h"
#include "../loaders/objexporter.h"
#include "../loaders/plyexporter.h"

namespace fs = std::filesystem;

// Forward declarations for hierarchical simplification
namespace nx {

// Step 3: Merge clusters within a micronode into a temporary mesh
struct MergedMesh {
    std::vector<Vector3f> positions;
    std::vector<Wedge> wedges;
    std::vector<Triangle> triangles;
    std::set<Index> boundary_vertices;  // Vertices on micronode boundary
    Index parent_micronode_id;
};

// Step 6: Result of splitting a simplified mesh
struct SplitResult {
    std::vector<Index> cluster_assignment;  // Triangle -> cluster ID (0 or 1)
    Index cluster_0_count;
    Index cluster_1_count;
};

// Merge 4 clusters from a micronode into a single mesh
MergedMesh merge_micronode_clusters(
    const MeshFiles& mesh,
    const MicroNode& micronode,
    const std::set<Index>& external_clusters);

// Identify and lock boundary edges (edges connecting to external clusters)
void identify_boundary_vertices(
    MergedMesh& merged,
    const MeshFiles& mesh,
    const MicroNode& micronode,
    const std::set<Index>& external_clusters);

// Simplify mesh to target triangle count (respecting locked boundaries)
void simplify_mesh(
    MergedMesh& mesh,
    Index target_triangle_count);

// Split simplified mesh into 2 clusters minimizing external edges
SplitResult split_into_two_clusters(
    const MergedMesh& mesh,
    const std::set<Index>& external_cluster_boundaries);

// Create new micronodes ensuring no two clusters from same parent are grouped
std::vector<MicroNode> create_next_level_micronodes(
    const std::vector<Index>& cluster_parent_micronode,
    Index num_new_clusters,
    const MeshFiles& mesh);

} // namespace nx

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: nxsbuild [--metis] [--metis-micro] <input_mesh>" << std::endl;
        std::cerr << "  --metis:       Use METIS for triangle clustering (default: greedy)" << std::endl;
        std::cerr << "  --metis-micro: Use METIS for micro-clustering (default: greedy)" << std::endl;
        return 1;
    }

    bool use_metis = false;
    bool use_metis_micro = false;
    std::string input_file;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--metis") {
            use_metis = true;
        } else if (arg == "--metis-micro") {
            use_metis_micro = true;
        } else {
            input_file = arg;
        }
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: No input mesh file specified" << std::endl;
        return 1;
    }
    nx::MeshFiles mesh;

    try {
        nx::load_mesh(fs::path(input_file), mesh);
        
        // Sort mesh spatially for memory coherency
        nx::spatial_sort_mesh(mesh);
        
        // Compute face-face adjacency
        nx::compute_adjacency(mesh);
        
        // Build clusters - choose algorithm
        std::size_t max_triangles = 512; 
        std::size_t cluster_count;
        
        if (use_metis) {
            std::cout << "\n*** Using METIS clustering algorithm ***\n" << std::endl;
            cluster_count = nx::build_clusters_metis(mesh, max_triangles);
        } else {
            std::cout << "\n*** Using greedy clustering algorithm ***\n" << std::endl;
            cluster_count = nx::build_clusters(mesh, max_triangles);
        }
        
        std::cout << "Built " << cluster_count << " clusters" << std::endl;

        // Create micronodes from clusters - choose algorithm
        std::size_t clusters_per_micronode = 4;
        
        if (use_metis_micro) {
            std::cout << "\n*** Using METIS micro-clustering algorithm ***\n" << std::endl;
            mesh.micronodes = nx::create_micronodes_metis(mesh, clusters_per_micronode);
        } else {
            std::cout << "\n*** Using greedy micro-clustering algorithm ***\n" << std::endl;
            mesh.micronodes = nx::create_micronodes(mesh, clusters_per_micronode);
        }

        // Create the micronodes adjacency graph + weights

        // Remove the edges internal to each micronode.

        // run create_micronodes_metis again to get the 'dual' micronodes grouping.

        // Measure the fraction of micronode sharing 3 edges with a 'dual' micronode.

      
        
        std::cout << "Created " << mesh.micronodes.size() << " micronodes" << std::endl;

        // ============================================================
        // Create primary and dual micronodes using METIS
        // ============================================================
        auto [primary_micronodes, dual_micronodes] = nx::create_primary_and_dual_micronodes(mesh, clusters_per_micronode);
        
        std::cout << "\nPrimary micronodes: " << primary_micronodes.size() << std::endl;
        std::cout << "Dual micronodes: " << dual_micronodes.size() << std::endl;
        
        // Update mesh.micronodes with primary partition
        mesh.micronodes = primary_micronodes;
        
        // ============================================================
        // Measure primary-dual overlap
        // ============================================================
        float overlap_fraction = nx::measure_micronode_overlap(mesh, primary_micronodes, dual_micronodes);
        
        // ============================================================
        // Multi-constraint reclustering
        // ============================================================
        std::cout << "\n=== Applying Multi-Constraint Reclustering ===" << std::endl;
        
        // This function modifies mesh in-place (reorders triangles, updates clusters)
        nx::recluster_mesh_multiconstraint(mesh, primary_micronodes, dual_micronodes, 128);


        fs::path ply_output = fs::current_path() / "test_export_clusters_reclustered.ply";
        nx::export_ply(mesh, ply_output, nx::ColoringMode::ByCluster);
        std::cout << "Exported reclustered PLY to: " << ply_output << std::endl;
        

        /*  Test export
        fs::path output_file = fs::current_path() / "test_export.obj";
        nx::export_obj(mesh, output_file);
        std::cout << "Exported test file to: " << output_file << std::endl;
        */
        

        // Level 0 is already created (clusters and micronodes)
        int level = 0;
        std::vector<nx::MicroNode> current_micronodes = mesh.micronodes;
        
        // Track parent micronode for each cluster (for constraint in step 7)
        std::vector<nx::Index> cluster_parent_micronode(mesh.clusters.size());
        for (nx::Index i = 0; i < current_micronodes.size(); ++i) {
            for (nx::Index cluster_id : current_micronodes[i].cluster_ids) {
                cluster_parent_micronode[cluster_id] = i;
            }
        }
        
        // Continue until we have very few clusters
        while (current_micronodes.size() > 1) {
            std::cout << "\n--- Level " << level << " -> Level " << (level + 1) << " ---" << std::endl;
            std::cout << "Current micronodes: " << current_micronodes.size() << std::endl;
            std::cout << "Current clusters: " << mesh.clusters.size() << std::endl;
            
            // TODO: Implement the following steps:
            
            // Step 1-2: For each micronode, merge its 4 clusters and identify boundaries
            std::cout << "TODO: Step 1-2: Merge clusters within micronodes and lock boundaries" << std::endl;
            
            // Step 3: Simplify each merged mesh to 50% triangles
            std::cout << "TODO: Step 3: Simplify merged meshes to 50% triangles" << std::endl;
            
            // Step 4: Split each simplified mesh into 2 new clusters
            std::cout << "TODO: Step 4: Split simplified meshes into 2 clusters each" << std::endl;
            
            // Step 5: Create new micronodes with parent-mixing constraint
            std::cout << "TODO: Step 5: Create new micronodes avoiding same-parent clusters" << std::endl;
            
            // For now, break to avoid infinite loop
            break;
        }
        
        std::cout << "\n=== Hierarchical Simplification Complete ===" << std::endl;
        
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Nexus v4 builder initialized." << std::endl;
    return 0;
}

// ============================================================
// HIERARCHICAL SIMPLIFICATION IMPLEMENTATION
// ============================================================

namespace nx {

MergedMesh merge_micronode_clusters(
    const MeshFiles& mesh,
    const MicroNode& micronode,
    const std::set<Index>& external_clusters) {
    
    MergedMesh merged;
    merged.parent_micronode_id = micronode.id;
    
    // TODO: Implement
    // 1. Collect all triangles from the 4 clusters
    // 2. Build a unified vertex/wedge array
    // 3. Remap triangle indices to the new unified arrays
    
    std::cout << "  [merge_micronode_clusters] TODO: Merge " 
              << micronode.cluster_ids.size() << " clusters" << std::endl;
    
    return merged;
}

void identify_boundary_vertices(
    MergedMesh& merged,
    const MeshFiles& mesh,
    const MicroNode& micronode,
    const std::set<Index>& external_clusters) {
    
    // TODO: Implement
    // 1. For each triangle in the merged mesh, check its adjacency
    // 2. If adjacent face belongs to external_clusters, mark edge vertices as boundary
    
    std::cout << "  [identify_boundary_vertices] TODO: Identify boundary" << std::endl;
}

void simplify_mesh(
    MergedMesh& mesh,
    Index target_triangle_count) {
    
    // TODO: Implement
    // 1. Use edge collapse with quadric error metrics
    // 2. Respect boundary_vertices (don't collapse boundary edges)
    // 3. Continue until target_triangle_count is reached
    
    std::cout << "  [simplify_mesh] TODO: Simplify to " 
              << target_triangle_count << " triangles" << std::endl;
}

SplitResult split_into_two_clusters(
    const MergedMesh& mesh,
    const std::set<Index>& external_cluster_boundaries) {
    
    SplitResult result;
    
    // TODO: Implement
    // 1. Build dual graph of triangles (adjacent triangles connected by edge)
    // 2. Use graph partitioning (e.g., METIS or greedy) to split into 2 parts
    // 3. Objective: minimize cut edges + minimize edges to external_cluster_boundaries
    
    std::cout << "  [split_into_two_clusters] TODO: Split mesh into 2 clusters" << std::endl;
    
    return result;
}

} // namespace nx

