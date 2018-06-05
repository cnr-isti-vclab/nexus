# Nxsbuild

Nxsbuild generates a .nxs multiresolution 3D model given one or more .ply file(s):

	nxsbuild [PLY INPUT FILE(S)] [OPTIONS]

The process involves generating a sequence of increasingly coarse partitions of the model, interleaved with simplifications steps.
If you need compression or streaming capabilities a pass through nxscompress/nxsedit is needed.

### Options

**-o <val>**  filename of the nexus output file
**-f <val>**  number of faces per patch, default 32768. Decreasing this value allow for better resolution control but reduces rendering performance and will increase size of the dataset. Small models with very large textures might require a small value
**-t <val>**  number of triangles in the top node, default 4096
**-d <val>**  decimation method [quadric, edgelen], default is quadric
**-s <val>**  decimation factor between levels, default 0.5
**-S <val>**  decimation skipped for n levels, default 0
**-O**  use original textures, no repacking
**-a <val>**  split nodes adaptively [0-1], default 0.333
**-v <val>**  vertex quantization grid size (might be approximated)
**-q <val>**  texture quality [0-100], default 92
**-p**  generate a multiresolution point cloud
**-N**  force per vertex normals, even in point clouds
**-n**  do not store per vertex normals
**-C**  save colors
**-c**  do not store per vertex colors
**-u**  do not store per vertex texture coordinates
**-r <val>**  max ram used (in MegaBytes), default 2000 (WARNING: not a hard limit, increase at your risk)
