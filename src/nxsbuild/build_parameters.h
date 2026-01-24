#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>

#include "../core/mesh_types.h"

namespace nx {

/**
 * @brief Texture compression format
 */
enum class TextureFormat {
	JPEG,      // Legacy lossy compression (universal support)
	PNG,       // Lossless compression (universal support)
	WebP,      // Modern lossy/lossless (Chrome, Firefox, Edge)
	Basis,     // Basis Universal / KTX2 (transcodes to GPU formats)
	BC7,       // DirectX BC7 / DXT (desktop GPU, high quality)
	ASTC,      // ARM Adaptive Scalable Texture Compression (mobile GPU)
	ETC2       // Ericsson Texture Compression 2 (OpenGL ES 3.0+)
};

/**
 * @brief Parameters for building a Nexus file
 * 
 * Encapsulates all command-line options and provides parsing logic
 */
class BuildParameters {
public:
	// Input/Output
	QStringList inputs;              // Input mesh files
	QString output;                  // Output .nxs filename
	QString mtl;                     // MTL file for OBJ

	// Construction options
	int faces_per_cluster = 1 << 8;
	int clusters_per_node = 4;
	int macro_node_faces = 1 << 15; // Faces per macro-node (32768)
	float texel_weight = 0.05f;      // Relative weight of texels
	float scaling = 0.5f;            // Decimation factor between levels

	// Clustering algorithm selection
	bool use_metis = true;           // Use METIS for triangle clustering

	// Texture format
	TextureFormat texture_format = TextureFormat::JPEG; // Texture compression format
	int texture_quality = 95;         // Quality for lossy formats [0-100]

	// Format options
	bool normals = false;            // Force per-vertex normals
	bool no_normals = false;         // Don't store normals
	bool colors = false;             // Save vertex colors
	bool no_colors = false;          // Don't store colors
	bool no_texcoords = false;       // Don't store texture coordinates

	bool deepzoom = false;           // Save each node to separate file

	// Transform options
	Vector3d translate = {0.0, 0.0, 0.0}; // New origin
	Vector3d scale = {1.0, 1.0, 1.0};     // Scale vector
	bool center = false;             // Center bounding box
	QString colormap;                // For .ts files: property:colormap

	// Performance options
	int num_threads = 0;             // Number of worker threads

	/**
     * @brief Default constructor with default values
	 */
	BuildParameters() = default;

	/**
     * @brief Parse command line arguments
     * @param argc Argument count
     * @param argv Argument vector
     * @return true if parsing succeeded, false on error
	 */
	bool parse(int argc, char* argv[]);

	/**
     * @brief Validate parameters
     * @return true if parameters are valid, false otherwise
	 */
	bool validate() const;

	/**
     * @brief Print usage information
	 */
	static void printUsage();

	/**
     * @brief Print parameter values
	 */
	void print() const;

private:
	/**
     * @brief Detect if input is a directory and expand to file list
	 */
	void expandDirectoryInputs();

	/**
     * @brief Set default output filename if not specified
	 */
	void setDefaultOutput();
};

} // namespace nx
