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
#include <map>
#include <iostream>
#include <cstdlib>
#include <cassert>
#include <numeric>
#include <metis.h>
#include "../core/log.h"

namespace nx {

namespace {

std::vector<Vector3f> compute_triangle_centroids(const MeshFiles& mesh) {
	std::size_t triangle_count = mesh.triangles.size();
	std::vector<Vector3f> centroids(triangle_count);
	for (std::size_t i = 0; i < triangle_count; ++i) {
		centroids[i] = compute_triangle_centroid(mesh.positions, mesh.wedges, mesh.triangles[i]);
	}
	return centroids;
}

void build_triangle_adjacency_graph(const MeshFiles& mesh,
								 const std::vector<Vector3f>& triangle_centroids,
								 std::vector<idx_t>& xadj,
								 std::vector<idx_t>& adjncy,
								 std::vector<idx_t>& adjwgt) {
	std::size_t num_triangles = mesh.triangles.size();

	xadj.reserve(num_triangles + 1);
	xadj.push_back(0);

	for (std::size_t i = 0; i < num_triangles; ++i) {
		const FaceAdjacency& adj = mesh.adjacency[i];
		const Vector3f& centroid_i = triangle_centroids[i];

		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor = adj.opp[corner];
			if (neighbor != std::numeric_limits<Index>::max()) {
				adjncy.push_back(static_cast<idx_t>(neighbor));

				const Vector3f& centroid_j = triangle_centroids[neighbor];
				float dx = centroid_i.x - centroid_j.x;
				float dy = centroid_i.y - centroid_j.y;
				float dz = centroid_i.z - centroid_j.z;
				float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

				idx_t weight = static_cast<idx_t>(std::max(1.0f, 1000.0f / (1.0f + distance)));
				adjwgt.push_back(weight);
			}
		}
		xadj.push_back(static_cast<idx_t>(adjncy.size()));
	}
	assert(xadj.size() == num_triangles + 1);
}

void build_clusters_from_partition(MeshFiles& mesh, std::size_t num_partitions) {
	std::size_t num_triangles = mesh.triangles.size();

	std::vector<Index> partition_sizes(num_partitions, 0);
	for (std::size_t i = 0; i < num_triangles; ++i) {
		Index cluster_id = mesh.triangle_to_cluster[i];
		partition_sizes[cluster_id]++;
	}

	mesh.clusters.resize(num_partitions);
	for (std::size_t i = 0; i < num_partitions; ++i) {
		Cluster cluster;
		cluster.triangle_offset = 0;
		cluster.triangle_count = partition_sizes[i];
		cluster.center = {0, 0, 0};
		cluster.radius = 0;
		mesh.clusters[i] = cluster;
	}
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

// FM Refinement - Fiduccia-Mattheyses pass with scoring and locking
void apply_fm_refinement(MeshFiles& mesh,
						 std::size_t max_size,
						 std::size_t max_passes) {
	if (mesh.clusters.size() == 0) return;

	std::size_t triangle_count = mesh.triangle_to_cluster.size();

	// Scoring weights
	const float w_topology = 1.0f;
	const float w_size = 0.01f;
	const float w_distance = 0.05f;

	// FM parameters
	const std::size_t BATCH_SIZE = 32768;
	const std::size_t MAX_CANDIDATES = 1024;

	bool improved = true;
	std::size_t pass = 0;

	while (improved && pass < max_passes) {
		improved = false;
		float pass_gain = 0.0f;

		// Process in batches
		for (std::size_t batch_start = 0; batch_start < triangle_count; batch_start += BATCH_SIZE) {
			std::size_t batch_end = std::min(batch_start + BATCH_SIZE, triangle_count);

			// Candidate structure for moves
			struct Candidate {
				Index triangle;
				Index source_cluster;
				Index target_cluster;
				float score;
			};

			std::vector<Candidate> candidates;

			// Find candidate moves in this batch
			for (Index tri_idx = batch_start; tri_idx < batch_end; ++tri_idx) {

				Index source_cluster = mesh.triangle_to_cluster[tri_idx];
				const Triangle& tri = mesh.triangles[tri_idx];

				// Find neighboring clusters via triangle adjacency
				std::set<Index> neighbors;
				const FaceAdjacency& adj = mesh.adjacency[tri_idx];
				for (int corner = 0; corner < 3; ++corner) {
					Index neighbor_tri = adj.opp[corner];
					if (neighbor_tri != std::numeric_limits<Index>::max()) {
						Index neighbor_cluster = mesh.triangle_to_cluster[neighbor_tri];
						if (neighbor_cluster != source_cluster) {
							neighbors.insert(neighbor_cluster);
						}
					}
				}

				// Score moves to each neighboring cluster
				for (Index target_cluster : neighbors) {

					// a) Topology: change in shared edges
					int shared_with_target = count_shared_edges_with_cluster(tri_idx, target_cluster,
																			 mesh.triangle_to_cluster, mesh.adjacency);
					int shared_with_source = count_shared_edges_with_cluster(tri_idx, source_cluster,
																			 mesh.triangle_to_cluster, mesh.adjacency);
					float g_topo = static_cast<float>(shared_with_target - shared_with_source);

					// b) Size: minimize squared distance from target size
					float target_size = static_cast<float>(max_size);
					float source_count = static_cast<float>(mesh.clusters[source_cluster].triangle_count);
					float target_count = static_cast<float>(mesh.clusters[target_cluster].triangle_count);

					// Squared distance from target before move
					float source_dist_before = (source_count - target_size) * (source_count - target_size);
					float target_dist_before = (target_count - target_size) * (target_count - target_size);

					// Squared distance from target after move (source loses 1, target gains 1)
					float source_dist_after = (source_count - 1.0f - target_size) * (source_count - 1.0f - target_size);
					float target_dist_after = (target_count + 1.0f - target_size) * (target_count + 1.0f - target_size);

					// Positive score when move reduces total distance from target
					float g_size = (source_dist_before + target_dist_before) - (source_dist_after + target_dist_after);

					// c) Distance: prefer moving toward nearby clusters
					Vector3f tri_centroid = compute_triangle_centroid(mesh.positions, mesh.wedges, tri);
					float dist_to_source = distance_to_triangle(mesh.positions, mesh.wedges, tri,
																mesh.clusters[source_cluster].center);
					float dist_to_target = distance_to_triangle(mesh.positions, mesh.wedges, tri,
																mesh.clusters[target_cluster].center);
					float g_dist = dist_to_source - dist_to_target;

					// Combined score
					float score = w_topology * g_topo + w_size * g_size - w_distance * g_dist;

					if (score > 0.0f) {
						candidates.push_back({tri_idx, source_cluster, target_cluster, score});
					}
				}
			}

			// Partially sort to get top candidates by score (descending)
			std::size_t num_to_sort = std::min(MAX_CANDIDATES, candidates.size());
			std::partial_sort(candidates.begin(),
							  candidates.begin() + num_to_sort,
							  candidates.end(),
							  [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

			// Apply top candidates with locking mechanism
			std::set<Index> locked_triangles;  // Prevent same triangle from being moved twice

			std::size_t applied = 0;
			for (const auto& cand : candidates) {
				if (locked_triangles.count(cand.triangle)) continue;

				// Apply the move
				Index old_cluster = mesh.triangle_to_cluster[cand.triangle];
				Index new_cluster = cand.target_cluster;

				mesh.triangle_to_cluster[cand.triangle] = new_cluster;
				mesh.clusters[old_cluster].triangle_count--;
				mesh.clusters[new_cluster].triangle_count++;

				pass_gain += cand.score;

				// Add neighbors of moved triangle to locked set
				// (they may now have better moves in future passes)
				const FaceAdjacency& moved_adj = mesh.adjacency[cand.triangle];
				for (int corner = 0; corner < 3; ++corner) {
					Index neighbor = moved_adj.opp[corner];
					if (neighbor != std::numeric_limits<Index>::max()) {
						locked_triangles.insert(neighbor);
					}
				}

				applied++;
				improved = true;
			}
		}

		// Stop if no improvement in this pass
		if (pass_gain <= 0.0f) break;

		pass++;
	}
}

} // anonymous namespace


// Print histogram of cluster triangle counts
void print_cluster_histogram(const MappedArray<Cluster>& clusters, const char* label) {
	if (clusters.size() == 0) return;

	nx::debug << "\n=== Cluster Triangle Count Histogram " << label << " ===" << std::endl;

	std::vector<int> histogram(10, 0);  // 10 buckets
	std::vector<size_t> triangle_counts(10, 0);  // Triangle count per bucket

	int min_count = std::numeric_limits<int>::max();
	int max_count = 0;
	size_t total_triangles = 0;
	size_t clusters_with_zero = 0;

	for (std::size_t i = 0; i < clusters.size(); ++i) {
		int count = clusters[i].triangle_count;
		min_count = std::min(min_count, count);
		max_count = std::max(max_count, count);
		total_triangles += count;
		if (count == 0) clusters_with_zero++;
	}

	if (min_count == std::numeric_limits<int>::max()) min_count = 0;

	nx::debug << "Total clusters: " << clusters.size() << ", Total triangles: " << total_triangles
			  << ", Clusters with 0 triangles: " << clusters_with_zero << std::endl;

	// Populate histogram
	for (std::size_t i = 0; i < clusters.size(); ++i) {
		int count = clusters[i].triangle_count;
		int bucket = (max_count > min_count)
						 ? std::min(9, static_cast<int>((count - min_count) * 10 / (max_count - min_count + 1)))
						 : 0;
		histogram[bucket]++;
		triangle_counts[bucket] += count;
	}

	// Print histogram
	int max_freq = *std::max_element(histogram.begin(), histogram.end());
	for (int b = 0; b < 10; ++b) {
		int lo = min_count + (max_count - min_count) * b / 10;
		int hi = min_count + (max_count - min_count) * (b + 1) / 10;
		int freq = histogram[b];
		size_t tri_count = triangle_counts[b];
		int bars = (max_freq > 0) ? (freq * 50 / max_freq) : 0;

		nx::debug << "[" << lo << "-" << hi << "): ";
		nx::debug << " (" << freq << " clusters, " << tri_count << " triangles)" << std::endl;
	}
}


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

// Reorder triangles to be contiguous by cluster and update cluster metadata
void reorder_triangles_by_cluster(MeshFiles& mesh) {
	if (mesh.clusters.size() == 0) return;

	std::size_t num_triangles = mesh.triangle_to_cluster.size();

	// Compute cluster offsets based on their triangle counts
	std::vector<Index> cluster_offsets(mesh.clusters.size());
	Index current_offset = 0;
	for (std::size_t c = 0; c < mesh.clusters.size(); ++c) {
		cluster_offsets[c] = current_offset;
		mesh.clusters[c].triangle_offset = current_offset;
		current_offset += mesh.clusters[c].triangle_count;
	}

	// Build new triangle array by scattering triangles into their cluster positions
	std::vector<Triangle> new_triangles(num_triangles);
	std::vector<Index> old_to_new(num_triangles);
	std::vector<Index> write_offsets = cluster_offsets;

	// Scatter triangles into their cluster bins
	for (Index tri_idx = 0; tri_idx < num_triangles; ++tri_idx) {
		Index cluster_id = mesh.triangle_to_cluster[tri_idx];
		Index write_pos = write_offsets[cluster_id];
		new_triangles[write_pos] = mesh.triangles[tri_idx];
		old_to_new[tri_idx] = write_pos;
		write_offsets[cluster_id]++;
	}

	// Write back reordered triangles
	for (std::size_t i = 0; i < num_triangles; ++i) {
		mesh.triangles[i] = new_triangles[i];
	}

	// Reorder adjacency to match new triangle order
	std::vector<FaceAdjacency> new_adjacency(num_triangles);
	for (Index old_idx = 0; old_idx < num_triangles; ++old_idx) {
		Index new_idx = old_to_new[old_idx];
		FaceAdjacency adj = mesh.adjacency[old_idx];
		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor = adj.opp[corner];
			if (neighbor != std::numeric_limits<Index>::max()) {
				adj.opp[corner] = old_to_new[neighbor];
			}
		}
		new_adjacency[new_idx] = adj;
	}
	mesh.adjacency.resize(num_triangles);
	for (std::size_t i = 0; i < num_triangles; ++i) {
		mesh.adjacency[i] = new_adjacency[i];
	}

	// Update triangle_to_cluster to match reordered triangles
	mesh.triangle_to_cluster.resize(num_triangles);
	for (std::size_t c = 0; c < mesh.clusters.size(); ++c) {
		const Cluster& cluster = mesh.clusters[c];
		for (Index i = 0; i < cluster.triangle_count; ++i) {
			mesh.triangle_to_cluster[cluster.triangle_offset + i] = static_cast<Index>(c);
		}
	}
}

// Compute bounds for each cluster after reordering
void compute_cluster_bounds(MeshFiles& mesh) {
	std::size_t cluster_count = mesh.clusters.size();
	if (cluster_count == 0) return;

	for (std::size_t c = 0; c < cluster_count; ++c) {
		Cluster& cluster = mesh.clusters[c];
		compute_bounding_sphere(cluster.triangle_offset, cluster.triangle_count,
								mesh, cluster.center, cluster.radius);
	}
}

void build_clusters_greedy(MeshFiles& mesh, std::size_t max_triangles) {
	if (max_triangles < 1 || max_triangles > 512) {
		throw std::runtime_error("max_triangles must be between 1 and 512");
	}

	std::size_t triangle_count = mesh.triangles.size();
	if (triangle_count == 0) return;

	// Triangle to cluster mapping
	mesh.triangle_to_cluster.assign(triangle_count, std::numeric_limits<Index>::max());

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
			assert(mesh.triangle_to_cluster[seed] == std::numeric_limits<Index>::max());

		} else {
			seed = std::numeric_limits<Index>::max();
			for (std::size_t i = 0; i < triangle_count; ++i) {
				if (mesh.triangle_to_cluster[i] == std::numeric_limits<Index>::max()) {
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
		mesh.triangle_to_cluster[seed] = current_cluster_id;
		triangles_processed++;
		front.erase(seed);

		// Compute initial cluster centroid
		Vector3f cluster_centroid = compute_triangle_centroid(mesh.positions, mesh.wedges, mesh.triangles[seed]);

		// Initialize boundary with seed neighbors
		const FaceAdjacency& seed_adj = mesh.adjacency[seed];
		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor = seed_adj.opp[corner];
			if (neighbor != std::numeric_limits<Index>::max() &&
				mesh.triangle_to_cluster[neighbor] == std::numeric_limits<Index>::max()) {
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
				assert(mesh.triangle_to_cluster[tri_idx] == std::numeric_limits<Index>::max());

				// Score: shared edges (higher is better) minus distance penalty
				int shared = count_shared_edges_with_cluster(tri_idx, current_cluster_id,
															 mesh.triangle_to_cluster, mesh.adjacency);
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
			mesh.triangle_to_cluster[best] = current_cluster_id;
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
					mesh.triangle_to_cluster[neighbor] == std::numeric_limits<Index>::max()) {
					boundary.insert(neighbor);
				}
			}
		}

		// Store cluster metadata
		Cluster cluster;
		cluster.triangle_offset = 0;  // Will be set after reordering
		cluster.triangle_count = cluster_triangle_count;
		cluster.center = cluster_centroid;  // Store computed centroid (will be overwritten with sphere after reordering)
		cluster.radius = 0;
		temp_clusters.push_back(cluster);

		// Update front: add new boundary neighbors
		for (Index tri_idx : boundary) {
			if (mesh.triangle_to_cluster[tri_idx] == std::numeric_limits<Index>::max()) {
				front.insert(tri_idx);
			}
		}

		current_cluster_id++;
	}

	// Copy temp clusters to mesh storage
	mesh.clusters.resize(temp_clusters.size());
	for (std::size_t i = 0; i < temp_clusters.size(); ++i) {
		mesh.clusters[i] = temp_clusters[i];
	}

	// Print histogram before refinement
	print_cluster_histogram(mesh.clusters, "(before FM refinement)");

	// Apply FM refinement with proper pass-based algorithm
	apply_fm_refinement(mesh, 128, 10);
}

void build_clusters_metis(MeshFiles& mesh, std::size_t max_triangles) {
	nx::debug << "\n=== Building clusters with METIS ===" << std::endl;

	std::size_t num_triangles = mesh.triangles.size();
	if (num_triangles == 0) {
		throw std::runtime_error("Error: No triangles to cluster");
	}

	// Estimate number of partitions
	std::size_t num_partitions = (num_triangles + max_triangles - 1) / max_triangles;
	if (num_partitions < 2) num_partitions = 2;


	std::vector<Vector3f> triangle_centroids = compute_triangle_centroids(mesh);

	// Build graph in CSR format for METIS with edge weights
	std::vector<idx_t> xadj;
	std::vector<idx_t> adjncy;
	std::vector<idx_t> adjwgt;

	build_triangle_adjacency_graph(mesh, triangle_centroids, xadj, adjncy, adjwgt);

	if (adjncy.empty()) {
		throw std::runtime_error("Error: Graph has no edges - mesh has no adjacency (pathological mesh)");
	}

	idx_t nvtxs = static_cast<idx_t>(num_triangles);
	idx_t ncon = 1;
	idx_t nparts = static_cast<idx_t>(num_partitions);
	idx_t objval;

	std::vector<idx_t> part(num_triangles);

	idx_t options[METIS_NOPTIONS];
	METIS_SetDefaultOptions(options);
	options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
	options[METIS_OPTION_NUMBERING] = 0;

	nx::debug << "Calling METIS_PartGraphKway with " << nvtxs << " vertices, "
			  << nparts << " partitions..." << std::endl;

	int ret = METIS_PartGraphKway(&nvtxs, &ncon,
		xadj.data(), adjncy.data(),
		nullptr,
		nullptr,
		adjwgt.data(),
		&nparts,
		nullptr,
		nullptr,
		options,
		&objval,
		part.data()
		);

	if (ret != METIS_OK) {
		throw std::runtime_error("METIS_PartGraphKway failed with code " + std::to_string(ret));
	}

	mesh.triangle_to_cluster.resize(num_triangles);
	for (std::size_t i = 0; i < num_triangles; ++i) {
		mesh.triangle_to_cluster[i] = static_cast<Index>(part[i]);
	}

	build_clusters_from_partition(mesh, num_partitions);
}

void build_clusters(MeshFiles& mesh, std::size_t max_triangles, ClusteringMethod method) {
	switch (method) {
		case ClusteringMethod::Greedy:
			build_clusters_greedy(mesh, max_triangles);
		break;
		case ClusteringMethod::Metis:
			build_clusters_metis(mesh, max_triangles);
		break;
	}
	reorder_triangles_by_cluster(mesh);
	compute_cluster_bounds(mesh);

	print_cluster_histogram(mesh.clusters, "(METIS output)");
}


void split_clusters(MeshFiles& mesh, std::size_t max_triangles) {
	nx::debug << "\n=== Splitting clusters into N and creating micronodes ===" << std::endl;

	std::size_t num_original_clusters = mesh.clusters.size();
	if (num_original_clusters == 0) {
		std::cerr << "Error: No clusters to split" << std::endl;
		return;
	}

	// For each original cluster, we'll split it into 4 sub-clusters
	// and create a micronode containing those 4 clusters

	std::vector<Cluster> new_clusters;
	std::vector<MicroNode> micronodes;
	std::vector<Index> new_triangle_to_cluster(mesh.triangles.size());

	new_clusters.reserve(num_original_clusters * 4);
	micronodes.reserve(num_original_clusters);

	for (std::size_t c = 0; c < num_original_clusters; ++c) {
		const Cluster& original = mesh.clusters[c];
		Index tri_offset = original.triangle_offset;
		Index tri_count = original.triangle_count;

		if (tri_count == 0) continue;

		// Collect triangle indices for this cluster
		std::vector<Index> cluster_triangles(tri_count);
		for (Index i = 0; i < tri_count; ++i) {
			cluster_triangles[i] = tri_offset + i;
		}

		// Determine number of sub-partitions (target 4, but may be fewer for small clusters)
		std::size_t num_sub_parts = 4;
		if (tri_count < 4) {
			num_sub_parts = tri_count;  // One cluster per triangle if < 4
		} else if (tri_count < max_triangles * 2) {
			num_sub_parts = 2;  // Only split in 2 if not enough triangles
		}

		std::vector<idx_t> sub_part(tri_count, 0);

		if (num_sub_parts > 1 && tri_count >= num_sub_parts) {
			// Build local graph for triangles in this cluster
			// Map global triangle index to local index
			std::map<Index, idx_t> global_to_local;
			for (Index i = 0; i < tri_count; ++i) {
				global_to_local[cluster_triangles[i]] = static_cast<idx_t>(i);
			}

			// Build CSR adjacency for local triangles
			std::vector<idx_t> xadj;
			std::vector<idx_t> adjncy;
			std::vector<idx_t> adjwgt;

			xadj.reserve(tri_count + 1);
			xadj.push_back(0);

			// Compute triangle centroids for edge weights
			std::vector<Vector3f> centroids(tri_count);
			for (Index i = 0; i < tri_count; ++i) {
				Index global_tri = cluster_triangles[i];
				const Triangle& tri = mesh.triangles[global_tri];
				Vector3f centroid = {0, 0, 0};
				for (int j = 0; j < 3; ++j) {
					const Vector3f& p = mesh.positions[mesh.wedges[tri.w[j]].p];
					centroid.x += p.x;
					centroid.y += p.y;
					centroid.z += p.z;
				}
				centroid.x /= 3.0f;
				centroid.y /= 3.0f;
				centroid.z /= 3.0f;
				centroids[i] = centroid;
			}

			for (Index i = 0; i < tri_count; ++i) {
				Index global_tri = cluster_triangles[i];
				const FaceAdjacency& adj = mesh.adjacency[global_tri];
				const Vector3f& centroid_i = centroids[i];

				for (int corner = 0; corner < 3; ++corner) {
					Index neighbor = adj.opp[corner];
					if (neighbor != std::numeric_limits<Index>::max()) {
						auto it = global_to_local.find(neighbor);
						if (it != global_to_local.end()) {
							// Neighbor is in the same cluster
							adjncy.push_back(it->second);

							const Vector3f& centroid_j = centroids[it->second];
							float dx = centroid_i.x - centroid_j.x;
							float dy = centroid_i.y - centroid_j.y;
							float dz = centroid_i.z - centroid_j.z;
							float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
							idx_t weight = static_cast<idx_t>(std::max(1.0f, 1000.0f / (1.0f + distance)));
							adjwgt.push_back(weight);
						}
					}
				}
				xadj.push_back(static_cast<idx_t>(adjncy.size()));
			}

			if (!adjncy.empty()) {
				// Call METIS to partition
				idx_t nvtxs = static_cast<idx_t>(tri_count);
				idx_t ncon = 1;
				idx_t nparts = static_cast<idx_t>(num_sub_parts);
				idx_t objval;

				idx_t options[METIS_NOPTIONS];
				METIS_SetDefaultOptions(options);
				options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
				options[METIS_OPTION_NUMBERING] = 0;

				int ret = METIS_PartGraphKway(
					&nvtxs, &ncon,
					xadj.data(), adjncy.data(),
					nullptr, nullptr, adjwgt.data(),
					&nparts, nullptr, nullptr,
					options, &objval, sub_part.data());

				if (ret != METIS_OK) {
					throw std::runtime_error("Failed splitting a cluster");
				}
			} else {
				// No edges - sequential assignment
				for (Index i = 0; i < tri_count; ++i) {
					sub_part[i] = static_cast<idx_t>(i * num_sub_parts / tri_count);
				}
			}
		}

		// Count actual partitions used
		idx_t max_part = 0;
		for (Index i = 0; i < tri_count; ++i) {
			max_part = std::max(max_part, sub_part[i]);
		}
		std::size_t actual_parts = static_cast<std::size_t>(max_part) + 1;

		// Create sub-clusters and assign triangles
		Index first_new_cluster = static_cast<Index>(new_clusters.size());
		std::vector<Index> sub_cluster_counts(actual_parts, 0);

		// Count triangles per sub-partition
		for (Index i = 0; i < tri_count; ++i) {
			sub_cluster_counts[sub_part[i]]++;
		}

		// Create new cluster entries
		for (std::size_t p = 0; p < actual_parts; ++p) {
			Cluster new_cluster;
			new_cluster.triangle_offset = 0;  // Will be set after reordering
			new_cluster.triangle_count = sub_cluster_counts[p];
			new_cluster.center = {0, 0, 0};
			new_cluster.radius = 0;
			new_clusters.push_back(new_cluster);
		}

		// Assign triangles to new clusters
		for (Index i = 0; i < tri_count; ++i) {
			Index global_tri = cluster_triangles[i];
			Index new_cluster_id = first_new_cluster + static_cast<Index>(sub_part[i]);
			new_triangle_to_cluster[global_tri] = new_cluster_id;
		}

		// Create micronode for this original cluster
		MicroNode micronode;
		micronode.id = static_cast<Index>(micronodes.size());
		micronode.triangle_count = tri_count;
		micronode.centroid = original.center;  // Use original cluster center as initial centroid
		micronode.center = original.center;
		micronode.radius = original.radius;

		for (std::size_t p = 0; p < actual_parts; ++p) {
			micronode.cluster_ids.push_back(first_new_cluster + static_cast<Index>(p));
		}

		micronodes.push_back(std::move(micronode));
	}

	// Replace clusters and triangle_to_cluster
	mesh.clusters.resize(new_clusters.size());
	for (std::size_t i = 0; i < new_clusters.size(); ++i) {
		mesh.clusters[i] = new_clusters[i];
	}
	mesh.triangle_to_cluster = std::move(new_triangle_to_cluster);

	// Store micronodes
	mesh.micronodes = std::move(micronodes);

	//TODO we know the triangles in each micronode are consecutive we could reorder the triangles
	//just after the split and save a second pass in memory
	//Reorder triangles by new clusters
	reorder_triangles_by_cluster(mesh);

	// Compute bounds for new clusters
	//TODO: same as before: do it after split and local reorder
	compute_cluster_bounds(mesh);

	// Update micronode centroids and bounds based on their clusters
	for (MicroNode& mn : mesh.micronodes) {
		mn.centroid = {0, 0, 0};
		mn.triangle_count = 0;
		float total_weight = 0.0f;

		for (Index cluster_id : mn.cluster_ids) {
			const Cluster& c = mesh.clusters[cluster_id];
			float weight = static_cast<float>(c.triangle_count);
			mn.centroid.x += c.center.x * weight;
			mn.centroid.y += c.center.y * weight;
			mn.centroid.z += c.center.z * weight;
			total_weight += weight;
			mn.triangle_count += c.triangle_count;
		}

		if (total_weight > 0.0f) {
			mn.centroid.x /= total_weight;
			mn.centroid.y /= total_weight;
			mn.centroid.z /= total_weight;
		}

		mn.center = mn.centroid;
		mn.radius = 0.0f;
		for (Index cluster_id : mn.cluster_ids) {
			const Cluster& c = mesh.clusters[cluster_id];
			float dx = c.center.x - mn.center.x;
			float dy = c.center.y - mn.center.y;
			float dz = c.center.z - mn.center.z;
			float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
			mn.radius = std::max(mn.radius, dist + c.radius);
		}
	}
}

} // namespace nx
