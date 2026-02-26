#ifndef NX_EXPORT_NXS_H
#define NX_EXPORT_NXS_H

#include <string>
#include "../common/dag.h"
#include <cstdint>
#include <vector>

namespace nx {

class MeshHierarchy;
class MappedMesh;
class MicroNode;


// Stub exporter for NXS format. Implementation will be expanded later.
class ExportNxs {
public:
	// Export mesh hierarchy to the given path. Returns true on success.
	void export_nxs(MeshHierarchy& hierarchy, const std::string& path);
private:

	FILE *out = nullptr;
	Header header;
	std::vector<Node> nodes;
	std::vector<Patch> patches;
	std::vector<Texture> textures;
	std::vector<std::vector<uint8_t>> texture_payloads;
	std::vector<int> level_node_offset;

	//from per level node and cluster ids to global
	void computeGlobalIndexing(MeshHierarchy &hierarchy);
	//fill children_nodes

	void exportMicronode(int level, std::uint32_t micronode_id, MappedMesh &mesh, FILE *out);
	void saturateNodes();
	void saturateNode(uint32_t n);
	void printDag(MeshHierarchy &hierarchy);
	void printDagHierarchy(MeshHierarchy &hierarchy);
	std::vector<uint8_t> encodeBaseColorJpeg(const MappedMesh& mesh, std::uint32_t  micronode_id, int quality = 92);
 
	void alignFile() {
		uint64_t pos = static_cast<uint64_t>(ftell(out));
		uint64_t aligned = pos;
		pad(aligned);
		uint64_t delta = aligned - pos;
		for(uint64_t i = 0; i < delta; ++i)
			fputc(0, out);
	}

	void pad(uint64_t &size) {
		uint64_t m = (size-1) & ~(NEXUS_PADDING -1);
		size = m  + NEXUS_PADDING;
	}
};

} // namespace nx

#endif // NX_EXPORT_NXS_H
