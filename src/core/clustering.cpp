#include "clustering.h"
#include "mesh.h"
#include "mesh_types.h"
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <queue>
#include <set>
#include <iostream>
#include <cstdlib>
#include <cassert>

namespace nx {

namespace {

// Helper: Compute centroid of a triangle
Vector3f compute_triangle_centroid(const MappedArray<Vector3f>& positions,
                                  const MappedArray<Wedge>& wedges,
                                  const Triangle& tri) {
    Vector3f centroid = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        const Vector3f& p = positions[wedges[tri.w[i]].p];
        centroid.x += p.x;
        centroid.y += p.y;
        centroid.z += p.z;
    }
    centroid.x /= 3.0f;
    centroid.y /= 3.0f;
    centroid.z /= 3.0f;
    return centroid;
}

// Helper: Distance from point to triangle centroid
float distance_to_triangle(const MappedArray<Vector3f>& positions,
                          const MappedArray<Wedge>& wedges,
                          const Triangle& tri,
                          const Vector3f& point) {
    Vector3f centroid = compute_triangle_centroid(positions, wedges, tri);
    float dx = centroid.x - point.x;
    float dy = centroid.y - point.y;
    float dz = centroid.z - point.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// Helper: Count shared edges between triangle and a cluster using adjacency
int count_shared_edges_with_cluster(Index tri_idx,
                                    Index cluster_id,
                                    const std::vector<Index>& triangle_to_cluster,
                                    const MappedArray<FaceAdjacency>& adjacency) {
    const FaceAdjacency& adj = adjacency[tri_idx];
    int shared = 0;
    
    for (int corner = 0; corner < 3; ++corner) {
        Index neighbor = adj.opp[corner];
        if (neighbor != std::numeric_limits<Index>::max() &&
            triangle_to_cluster[neighbor] == cluster_id) {
            shared++;
        }
    }
    
    return shared;
}

// Helper: Find first unprocessed triangle in the mesh
Index find_first_unprocessed(const std::vector<bool>& triangle_used, std::size_t triangle_count) {
    for (std::size_t i = 0; i < triangle_count; ++i) {
        if (!triangle_used[i]) return static_cast<Index>(i);
    }
    return std::numeric_limits<Index>::max();
}

// Helper: Find first unprocessed triangle from the front
Index find_seed_from_front(const std::set<Index>& front, const std::vector<bool>& triangle_used) {
    for (Index tri : front) {
        if (!triangle_used[tri]) {
            return tri;
        }
    }
    return std::numeric_limits<Index>::max();
}

// Compute bounding sphere from contiguous cluster triangles using Ritter's algorithm
void compute_bounding_sphere(Index triangle_offset, Index triangle_count,
                            const MeshFiles& mesh,
                            Vector3f& center, float& radius) {
    if (triangle_count == 0) {
        center = {0, 0, 0};
        radius = 0;
        return;
    }
    
    // Collect unique vertices from triangles
    std::set<Index> unique_vertices;
    for (Index t = triangle_offset; t < triangle_offset + triangle_count; ++t) {
        const Triangle& tri = mesh.triangles[t];
        for (int j = 0; j < 3; ++j) {
            unique_vertices.insert(mesh.wedges[tri.w[j]].p);
        }
    }
    
    // Find approximate extremes
    auto it = unique_vertices.begin();
    Vector3f min_pt = mesh.positions[*it];
    Vector3f max_pt = mesh.positions[*it];
    
    for (Index vi : unique_vertices) {
        const Vector3f& p = mesh.positions[vi];
        if (p.x < min_pt.x) min_pt = p;
        if (p.x > max_pt.x) max_pt = p;
    }
    
    // Initial center and radius
    center.x = (min_pt.x + max_pt.x) * 0.5f;
    center.y = (min_pt.y + max_pt.y) * 0.5f;
    center.z = (min_pt.z + max_pt.z) * 0.5f;
    
    float dx = max_pt.x - min_pt.x;
    float dy = max_pt.y - min_pt.y;
    float dz = max_pt.z - min_pt.z;
    radius = std::sqrt(dx*dx + dy*dy + dz*dz) * 0.5f;
    
    // Expand to include all points
    for (Index vi : unique_vertices) {
        const Vector3f& p = mesh.positions[vi];
        dx = p.x - center.x;
        dy = p.y - center.y;
        dz = p.z - center.z;
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (dist > radius) {
            radius = (radius + dist) * 0.5f;
            float scale = (dist > 0) ? (dist - radius) / dist : 0;
            center.x += dx * scale;
            center.y += dy * scale;
            center.z += dz * scale;
        }
    }
}

// Print histogram of cluster triangle counts
void print_cluster_histogram(const MappedArray<Cluster>& clusters, const char* label = "") {
    if (clusters.size() == 0) return;
    
    std::cout << "\n=== Cluster Triangle Count Histogram " << label << " ===" << std::endl;
    
    std::vector<int> histogram(10, 0);  // 10 buckets
    int min_count = std::numeric_limits<int>::max();
    int max_count = 0;
    
    for (std::size_t i = 0; i < clusters.size(); ++i) {
        int count = clusters[i].triangle_count;
        min_count = std::min(min_count, count);
        max_count = std::max(max_count, count);
    }
    
    if (min_count == std::numeric_limits<int>::max()) min_count = 0;
    
    // Populate histogram
    for (std::size_t i = 0; i < clusters.size(); ++i) {
        int count = clusters[i].triangle_count;
        int bucket = (max_count > min_count) 
            ? std::min(9, static_cast<int>((count - min_count) * 10 / (max_count - min_count + 1)))
            : 0;
        histogram[bucket]++;
    }
    
    // Print histogram
    int max_freq = *std::max_element(histogram.begin(), histogram.end());
    for (int b = 0; b < 10; ++b) {
        int lo = min_count + (max_count - min_count) * b / 10;
        int hi = min_count + (max_count - min_count) * (b + 1) / 10;
        int freq = histogram[b];
        int bars = (max_freq > 0) ? (freq * 50 / max_freq) : 0;
        
        std::cout << "[" << lo << "-" << hi << "]: ";
        for (int i = 0; i < bars; ++i) std::cout << "#";
        std::cout << " (" << freq << ")" << std::endl;
    }
}

// FM Refinement stub - moves triangles between clusters to improve partition quality
void apply_fm_refinement(MeshFiles& mesh,
                        std::vector<Index>& triangle_to_cluster,
                        std::size_t max_size,
                        std::size_t max_passes) {
    // TODO: Implement Fiduccia-Mattheyses refinement
    // For now, this is a stub that preserves the current partition
}

// Reorder triangles to be contiguous by cluster and update cluster metadata
void reorder_triangles_by_cluster(std::vector<Index>& triangle_to_cluster,
                                  MeshFiles& mesh) {
    if (mesh.clusters.size() == 0) return;
    
    std::size_t num_triangles = triangle_to_cluster.size();
    
    // Build temporary arrays to hold reordered data
    std::vector<Triangle> new_triangles;
    std::vector<Index> old_to_new_triangle_idx(num_triangles);
    std::vector<Index> new_triangle_to_cluster;
    
    // Reorder triangles by cluster
    Index current_offset = 0;
    for (std::size_t c = 0; c < mesh.clusters.size(); ++c) {
        mesh.clusters[c].triangle_offset = current_offset;
        mesh.clusters[c].triangle_count = 0;
        
        // Collect all triangles belonging to this cluster
        for (Index tri_idx = 0; tri_idx < num_triangles; ++tri_idx) {
            if (triangle_to_cluster[tri_idx] == static_cast<Index>(c)) {
                new_triangles.push_back(mesh.triangles[tri_idx]);
                new_triangle_to_cluster.push_back(static_cast<Index>(c));
                old_to_new_triangle_idx[tri_idx] = current_offset + mesh.clusters[c].triangle_count;
                mesh.clusters[c].triangle_count++;
            }
        }
        
        current_offset += mesh.clusters[c].triangle_count;
    }
    
    // Write back reordered triangles and update mapping
    for (std::size_t i = 0; i < new_triangles.size(); ++i) {
        mesh.triangles[i] = new_triangles[i];
    }
    triangle_to_cluster = new_triangle_to_cluster;
}

// Compute bounds for each cluster after reordering
void compute_cluster_bounds(MeshFiles& mesh) {
    std::size_t cluster_count = mesh.clusters.size();
    if (cluster_count == 0) return;
    
    // Resize cluster_bounds if needed
    if (mesh.cluster_bounds.size() < cluster_count) {
        mesh.cluster_bounds.resize(cluster_bounds_bytes(cluster_count));
    }
    
    for (std::size_t c = 0; c < cluster_count; ++c) {
        const Cluster& cluster = mesh.clusters[c];
        compute_bounding_sphere(cluster.triangle_offset, cluster.triangle_count,
                               mesh, mesh.cluster_bounds[c].center, mesh.cluster_bounds[c].radius);
    }
}

} // anonymous namespace

std::size_t build_clusters(MeshFiles& mesh, std::size_t max_triangles) {
    if (max_triangles < 1 || max_triangles > 512) {
        throw std::runtime_error("max_triangles must be between 1 and 512");
    }
    
    std::size_t triangle_count = mesh.triangles.size();
    if (triangle_count == 0) return 0;
    
    // Triangle to cluster mapping
    std::vector<Index> triangle_to_cluster(triangle_count, std::numeric_limits<Index>::max());
    
    // Temporary clusters vector (will be written to mesh.clusters after reordering)
    std::vector<Cluster> temp_clusters;
    
    // Front of boundary triangles for next cluster seed selection
    std::set<Index> front;
    
    // Build clusters greedily with spatial coherence
    std::size_t triangles_processed = 0;
    Index current_cluster_id = 0;
    
    while (triangles_processed < triangle_count) {
        // Pick seed: preferentially from front (spatial coherence), otherwise first unprocessed
        Index seed;
        if (!front.empty()) {
            seed = *front.begin();
            assert(triangle_to_cluster[seed] == std::numeric_limits<Index>::max());

        } else {
            seed = std::numeric_limits<Index>::max();
            for (std::size_t i = 0; i < triangle_count; ++i) {
                if (triangle_to_cluster[i] == std::numeric_limits<Index>::max()) {
                    seed = static_cast<Index>(i);
                    break;
                }
            }
        }
        //we have exhausted all triangles in this case and triangle_processed says we haven't
        assert(seed != std::numeric_limits<Index>::max());
        
        // Build a new cluster starting from seed
        Index cluster_triangle_count = 0;
        std::set<Index> boundary; // Active boundary of unprocessed adjacent triangles
        
        // Add seed triangle
        cluster_triangle_count++;
        triangle_to_cluster[seed] = current_cluster_id;
        triangles_processed++;
        front.erase(seed);
        
        // Compute initial cluster centroid
        Vector3f cluster_centroid = compute_triangle_centroid(mesh.positions, mesh.wedges, mesh.triangles[seed]);
        
        // Initialize boundary with seed neighbors
        const FaceAdjacency& seed_adj = mesh.adjacency[seed];
        for (int corner = 0; corner < 3; ++corner) {
            Index neighbor = seed_adj.opp[corner];
            if (neighbor != std::numeric_limits<Index>::max() && 
                triangle_to_cluster[neighbor] == std::numeric_limits<Index>::max()) {
                boundary.insert(neighbor);
            }
        }
        
        // Greedily grow the cluster
        while (cluster_triangle_count < max_triangles && !boundary.empty()) {
            // Find best candidate from boundary
            Index best = std::numeric_limits<Index>::max();
            float best_score = -std::numeric_limits<float>::max();
            
            for (Index tri_idx : boundary) {
                //we add triangles in boyundary only if they are unprocessed
                assert(triangle_to_cluster[tri_idx] == std::numeric_limits<Index>::max());
                
                // Score: shared edges (higher is better) minus distance penalty
                int shared = count_shared_edges_with_cluster(tri_idx, current_cluster_id, 
                                                            triangle_to_cluster, mesh.adjacency);
                float distance = distance_to_triangle(mesh.positions, mesh.wedges, 
                                                     mesh.triangles[tri_idx], cluster_centroid);
                float score = static_cast<float>(shared) - 0.05f * distance;
                
                if (score > best_score) {
                    best_score = score;
                    best = tri_idx;
                }
            }
            
            if (best == std::numeric_limits<Index>::max()) break;
            
            // Add the triangle
            cluster_triangle_count++;
            triangle_to_cluster[best] = current_cluster_id;
            triangles_processed++;
            boundary.erase(best);
            front.erase(best);
            
            // Update cluster centroid (incremental)
            Vector3f tri_centroid = compute_triangle_centroid(mesh.positions, mesh.wedges, mesh.triangles[best]);
            float n = static_cast<float>(cluster_triangle_count);
            cluster_centroid.x = (cluster_centroid.x * (n - 1) + tri_centroid.x) / n;
            cluster_centroid.y = (cluster_centroid.y * (n - 1) + tri_centroid.y) / n;
            cluster_centroid.z = (cluster_centroid.z * (n - 1) + tri_centroid.z) / n;
            
            // Expand boundary
            const FaceAdjacency& adj = mesh.adjacency[best];
            for (int corner = 0; corner < 3; ++corner) {
                Index neighbor = adj.opp[corner];
                if (neighbor != std::numeric_limits<Index>::max() && 
                    triangle_to_cluster[neighbor] == std::numeric_limits<Index>::max()) {
                    boundary.insert(neighbor);
                }
            }
        }
        
        // Store cluster metadata
        Cluster cluster;
        cluster.triangle_offset = 0;  // Will be set after reordering
        cluster.triangle_count = cluster_triangle_count;
        temp_clusters.push_back(cluster);
        
        // Update front: add new boundary neighbors
        for (Index tri_idx : boundary) {
            if (triangle_to_cluster[tri_idx] == std::numeric_limits<Index>::max()) {
                front.insert(tri_idx);
            }
        }
        
        current_cluster_id++;
    }
    
    // Copy temp clusters to mesh storage
    mesh.clusters.resize(clusters_bytes(temp_clusters.size()));
    for (std::size_t i = 0; i < temp_clusters.size(); ++i) {
        mesh.clusters[i] = temp_clusters[i];
    }
    
    // Print histogram before refinement
    print_cluster_histogram(mesh.clusters, "(before FM refinement)");
    
    // Apply FM refinement with proper pass-based algorithm
    apply_fm_refinement(mesh, triangle_to_cluster, 128, 5);
    
    // Print histogram after refinement
    print_cluster_histogram(mesh.clusters, "(after FM refinement)");
    
    // Reorder triangles to be contiguous by cluster
    reorder_triangles_by_cluster(triangle_to_cluster, mesh);
    
    // Compute bounds after reordering and FM refinement
    compute_cluster_bounds(mesh);
    
    // Write cluster data to mesh files
    mesh.clusters.resize(clusters_bytes(mesh.clusters.size()));
    mesh.cluster_bounds.resize(cluster_bounds_bytes(mesh.cluster_bounds.size()));
    
    return mesh.clusters.size();
}

} // namespace nx
