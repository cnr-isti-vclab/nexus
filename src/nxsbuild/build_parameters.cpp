#include "build_parameters.h"
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QFileInfo>
#include <iostream>

namespace nx {

bool BuildParameters::parse(int argc, char* argv[]) {
	QCommandLineParser parser;
	parser.setApplicationDescription("Build Nexus 3D files from triangle meshes");
	parser.addHelpOption();
	parser.addVersionOption();

	// Input/Output options
	parser.addOption({{"o", "output"}, "Output file (.nxs)", "file", ""});
	parser.addOption({{"m", "mtl"}, "MTL file (for .obj)", "file", ""});

	// Construction options
	parser.addOption({{"f", "faces"}, "Number of faces per cluster [256]", "n", "128"});
	parser.addOption({{"p", "clusters"}, "Number of clusters per node [4]", "n", "8"});
	parser.addOption({{"F", "macro-faces"}, "Number of faces per macro-node [32768]", "n", "32768"});
	parser.addOption({{"w", "texel-weight"}, "Relative weight for texels [0.05]", "f", "0.05"});
	parser.addOption({{"s", "scaling"}, "Decimation factor [0.5]", "f", "0.5"});

	// Clustering algorithm
	parser.addOption({"greedy", "Use greedy algorithm for triangle clustering"});

	// Compression options
	parser.addOption({{"q", "vertex-quantization"}, "Vertex quantization [0]", "f", "0"});
	parser.addOption({{"T", "texture-format"}, "Texture format: jpeg|webp|basis|bc7|etc2 [jpeg]", "format", "jpeg"});
	parser.addOption({{"Q", "texture-quality"}, "Quality for lossy formats [95]", "n", "95"});

	// Format options
	parser.addOption({{"n", "normals"}, "Force per-vertex normals"});
	parser.addOption({"no-normals", "Don't store normals"});
	parser.addOption({{"c", "colors"}, "Save vertex colors"});
	parser.addOption({"no-colors", "Don't store colors"});
	parser.addOption({"no-texcoords", "Don't store texture coordinates"});
	parser.addOption({{"d", "deepzoom"}, "Save each node to separate file"});

	// Transform options
	parser.addOption({"translate", "New origin X:Y:Z", "X:Y:Z", ""});
	parser.addOption({"scale", "Scale vector X:Y:Z", "X:Y:Z", ""});
	parser.addOption({"center", "Center bounding box"});
	parser.addOption({"colormap", "For .ts files: property:colormap", "prop:cm", ""});

	// Performance options
	parser.addOption({{"j", "threads"}, "Number of threads [4]", "n", "4"});
	parser.addOption({{"V", "verbosity"}, "Logging verbosity: silent|default|verbose", "level", "default"});

	// Add positional arguments
	parser.addPositionalArgument("inputs", "Input mesh file(s)", "<input files>");

	// Process arguments
	if (!parser.parse(QStringList::fromVector(QVector<QString>::fromList(
			QList<QString>() << QString::fromUtf8(argv[0])
			)))) {
		std::cerr << parser.errorText().toStdString() << std::endl;
		return false;
	}

	// Build arg list properly
	QStringList args;
	for (int i = 0; i < argc; ++i) {
		args << QString::fromUtf8(argv[i]);
	}

	if (!parser.parse(args)) {
		std::cerr << parser.errorText().toStdString() << std::endl;
		return false;
	}

	if (parser.isSet("help")) {
		parser.showHelp(0);
	}

	if (parser.isSet("version")) {
		parser.showVersion();
	}

	// Parse values
	output = parser.value("output");
	mtl = parser.value("mtl");
	faces_per_cluster = parser.value("faces").toInt();
	clusters_per_node = parser.value("clusters").toInt();
	macro_node_faces = parser.value("macro-faces").toInt();
	texel_weight = parser.value("texel-weight").toFloat();
	scaling = parser.value("scaling").toFloat();

	use_greedy = parser.isSet("greedy");

	// Parse texture format
	QString formatStr = parser.value("texture-format").toLower();
	if (formatStr == "jpeg" || formatStr == "jpg") {
		texture_format = TextureFormat::JPEG;
	} else if (formatStr == "png") {
		texture_format = TextureFormat::PNG;
	} else if (formatStr == "webp") {
		texture_format = TextureFormat::WebP;
	} else if (formatStr == "basis" || formatStr == "ktx2") {
		texture_format = TextureFormat::Basis;
	} else if (formatStr == "bc7" || formatStr == "dxt") {
		texture_format = TextureFormat::BC7;
	} else if (formatStr == "astc") {
		texture_format = TextureFormat::ASTC;
	} else if (formatStr == "etc2" || formatStr == "etc") {
		texture_format = TextureFormat::ETC2;
	} else {
		std::cerr << "Warning: unknown texture format '" << formatStr.toStdString()
				  << "', using JPEG\n";
		texture_format = TextureFormat::JPEG;
	}

	texture_quality = parser.value("texture-quality").toInt();

	normals = parser.isSet("normals");
	no_normals = parser.isSet("no-normals");
	colors = parser.isSet("colors");
	no_colors = parser.isSet("no-colors");
	no_texcoords = parser.isSet("no-texcoords");
	deepzoom = parser.isSet("deepzoom");

	// Parse translate vector (X:Y:Z format)
	QString translateStr = parser.value("translate");
	if (!translateStr.isEmpty()) {
		QStringList parts = translateStr.split(':');
		if (parts.size() == 3) {
			translate.x = parts[0].toDouble();
			translate.y = parts[1].toDouble();
			translate.z = parts[2].toDouble();
		} else {
			std::cerr << "Warning: invalid translate format '" << translateStr.toStdString()
					  << "', expected X:Y:Z\n";
		}
	}

	// Parse scale vector (X:Y:Z format)
	QString scaleStr = parser.value("scale");
	if (!scaleStr.isEmpty()) {
		QStringList parts = scaleStr.split(':');
		if (parts.size() == 3) {
			scale.x = parts[0].toDouble();
			scale.y = parts[1].toDouble();
			scale.z = parts[2].toDouble();
		} else {
			std::cerr << "Warning: invalid scale format '" << scaleStr.toStdString()
					  << "', expected X:Y:Z\n";
		}
	}

	center = parser.isSet("center");
	colormap = parser.value("colormap");

	num_threads = parser.value("threads").toInt();
	QString verbosityStr = parser.value("verbosity").toLower();
	if (verbosityStr == "silent") {
		verbosity = Verbosity::Silent;
	} else if (verbosityStr == "default") {
		verbosity = Verbosity::Default;
	} else if (verbosityStr == "verbose") {
		verbosity = Verbosity::Verbose;
	} else {
		std::cerr << "Warning: unknown verbosity '" << verbosityStr.toStdString()
				  << "', using default\n";
		verbosity = Verbosity::Default;
	}

	// Get positional arguments (input files)
	inputs = parser.positionalArguments();

	if (inputs.isEmpty()) {
		std::cerr << "Error: no input files specified\n";
		parser.showHelp(1);
		return false;
	}

	// Expand directories
	expandDirectoryInputs();

	// Set default output if not specified
	if (output.isEmpty()) {
		setDefaultOutput();
	}

	// Validate parameters
	if (!validate()) {
		return false;
	}

	return true;
}

bool BuildParameters::validate() const {
	// Validate node_faces range
	if (faces_per_cluster < 64 || faces_per_cluster > 2048) {
		std::cerr << "Error: faces per cluster must be between 64 and 2048\n";
		return false;
	}

	// Validate texture quality
	if (texture_quality < 70 || texture_quality > 100) {
		std::cerr << "Error: texture quality must be between 70 and 100\n";
		return false;
	}

	// Validate scaling factor
	if (scaling <= 0.2f || scaling >= 0.8f) {
		std::cerr << "Error: scaling factor must be between 0.2 and 0.8\n";
		return false;
	}

	// Validate thread count
	if (num_threads < 1) {
		std::cerr << "Error: thread count must be at least 1\n";
		return false;
	}

	// Check output file extension
	if (!output.endsWith(".nxs") && !output.endsWith(".nxz")) {
		std::cerr << "Warning: output file should have .nxs or .nxz extension\n";
	}

	return true;
}

void BuildParameters::printUsage() {
	std::cout << "\nUsage: nxsbuild [options] <input files>\n\n";
	std::cout << "Construction options:\n";
	std::cout << "  -f, --faces <n>           Faces per cluster [256]\n";
	std::cout << "  -c, --clusters <n>        Clusters per node [4]\n";
	std::cout << "  -F, --macro-faces <n>     Faces per macro-node [32768]\n";
	std::cout << "  -w, --texel-weight <f>    Relative weight for texels [0.05]\n";
	std::cout << "  -s, --scaling <f>         Decimation factor [0.5]\n";
	std::cout << "      --metis               Use METIS for triangle clustering\n\n";

	std::cout << "Texture format:\n";
	std::cout << "  -T, --texture-format <format>  Texture format: jpeg|png|webp|basis|bc7|astc|etc2 [jpeg]\n";
	std::cout << "  -Q, --texture-quality <n>      Quality for lossy formats 0-100 [95]\n\n";

	std::cout << "Format options:\n";
	std::cout << "  -n, --normals             Force per-vertex normals\n";
	std::cout << "      --no-normals          Don't store normals\n";
	std::cout << "  -c, --colors              Save vertex colors\n";
	std::cout << "      --no-colors           Don't store colors\n";
	std::cout << "      --no-texcoords        Don't store texture coordinates\n";
	std::cout << "  -d, --deepzoom            Save each node to separate file\n\n";

	std::cout << "Transform options:\n";
	std::cout << "      --translate <X:Y:Z>   New origin\n";
	std::cout << "      --scale <X:Y:Z>       Scale vector\n";
	std::cout << "      --center              Center bounding box\n";
	std::cout << "      --colormap <prop:cm>  For .ts files\n\n";

	std::cout << "Performance options:\n";
	std::cout << "  -j, --threads <n>         Number of threads [4]\n\n";
	std::cout << "  -v, --verbosity <level>   Logging verbosity (silent|default|verbose)\n\n";

	std::cout << "Input/Output:\n";
	std::cout << "  -o, --output <file>       Output file (.nxs)\n";
	std::cout << "  -m, --mtl <file>          MTL file (for .obj)\n\n";
}

void BuildParameters::print() const {
	nx::log << "\nBuild Parameters:\n";
	nx::log << "  Input files: " << inputs.size() << " file(s)\n";
	nx::log << "  Output: " << output.toStdString() << "\n";
	nx::log << "  Faces per cluster: " << faces_per_cluster << "\n";
	nx::log << "  Clusters per node: " << clusters_per_node << "\n";
	nx::log << "  Faces per macro-node: " << macro_node_faces << "\n";
	nx::log << "  Scaling: " << scaling << "\n";
	nx::log << "  Texel weight: " << texel_weight << "\n";

	// Print texture format
	nx::log << "  Texture format: ";
	switch (texture_format) {
	case TextureFormat::JPEG: nx::log << "JPEG"; break;
	case TextureFormat::PNG: nx::log << "PNG"; break;
	case TextureFormat::WebP: nx::log << "WebP"; break;
	case TextureFormat::Basis: nx::log << "Basis Universal"; break;
	case TextureFormat::BC7: nx::log << "BC7"; break;
	case TextureFormat::ASTC: nx::log << "ASTC"; break;
	case TextureFormat::ETC2: nx::log << "ETC2"; break;
	}
	nx::log << " (quality: " << texture_quality << ")\n";

	if (use_greedy) nx::log << "  Using greedy clustering\n";
	nx::log << "  Threads: " << num_threads << "\n";
	nx::log << "  Translate: (" << translate.x << ", " << translate.y << ", " << translate.z << ")\n";
	nx::log << "  Scale: (" << scale.x << ", " << scale.y << ", " << scale.z << ")\n";
	nx::log << "\n";
}

void BuildParameters::expandDirectoryInputs() {
	QStringList expanded;

	for (const QString& input : inputs) {
		QFileInfo info(input);
		if (info.isDir()) {
			QDir dir(input);
			QStringList filters;
			filters << "*.ply" << "*.obj" << "*.stl" << "*.off";
			QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
			for (const QFileInfo& file : files) {
				expanded.append(file.absoluteFilePath());
			}
		} else {
			expanded.append(input);
		}
	}

	inputs = expanded;
}

void BuildParameters::setDefaultOutput() {
	if (inputs.isEmpty()) {
		output = "output.nxs";
		return;
	}

	// Use first input file name with .nxs extension
	QFileInfo info(inputs.first());
	output = info.completeBaseName() + ".nxs";
}

} // namespace nx
