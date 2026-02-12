#include "meshloader.h"
#include "objloader.h"
#include "plyloader.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace nx {

void load_mesh(const std::filesystem::path& input_path, MappedMesh& mesh) {
	auto ext = input_path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".obj") {
		ObjLoader loader(input_path.string(), "");
		loader.load(mesh);
	} else if(ext == ".ply") {
		PlyLoader loader(input_path.string());
		loader.load(mesh);
	} else
		throw std::runtime_error("unsupported mesh format: " + ext);

	// TODO: add  glTF, and other loaders here when available.
}

void trimWhitespace(std::string& s) {
	// 1. Define the targets
	const std::string whitespace = " \t\n\r\f\v";

	// 2. Locate the last character that is NOT in the whitespace set
	size_t last = s.find_last_not_of(whitespace);

	// 3. Deductive logic:
	// If last == npos, the string is all whitespace. Erase everything.
	// If last < s.length() - 1, erase from last + 1 to the end.
	if (last == std::string::npos) {
		s.clear();
	} else {
		s.erase(last + 1);
	}
}


// Helper to sanitize and resolve texture paths
std::string resolveTexturePath(const std::string& model_path, std::string texture_path) {
	// Remove quotes
	texture_path.erase(std::remove(texture_path.begin(), texture_path.end(), '\"'), texture_path.end());

	fs::path tex_path(texture_path);
	if (tex_path.is_absolute()) {
		return texture_path;
	}

	fs::path model_dir = fs::path(model_path).parent_path();
	fs::path resolved = model_dir / tex_path;

	if (fs::exists(resolved)) {
		return resolved.string();
	}

	return texture_path;
}
void sanitizeTextureFilepath (std::string &textureFilepath) {
	trimWhitespace(textureFilepath);
	std::replace( textureFilepath.begin(), textureFilepath.end(), '\\', '/' );
}

} // namespace nx
