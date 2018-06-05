# Nxsedit

Nxsedit can be used for a number of post-processing operations over a nexus file:

	nxsedit [NEXUS INPUT FILE] [OPTIONS]

All operations generate a new nexus leaving the old one untouched.

### Options

**-i**: prints info about the nexus: number of patches, bounding sphere, etc. The number of triangles and vertices includes all the resolutions, you can assume the highest resolution level holds half the total amount

**-o <FILE>**: filename of the nexus output file, .nxs is added automatically

**-p <FILE>**: filename of the ply output file

**-s <val>**: extract a subset of the nxs of a given size (in MB), pruning lower error nodes [requires -o] 

**-e <val>**: extract a subset of the nxs pruning nodes with error below the given value [requires -o] 

**-t <val>**: extract a subset of the nxs of a given triangle count, pruning lower error nodes [requires -o] 

**-l**: prune the last level of nodes from the nexus (thus approximately halving the size and the triangle count) [requires -o] 

**-z**: applies a (somewhat lossy) compression algorithm to each patch

**-Z <val>**  pick among compression libs [corto, meco], default corto [requires -z]

**-v <val>**  absolute side of quantization grid [requires -z]

**-V <val>**  number of bits in vertex coordinates when compressing [requires -z]

**-Y <val>**  quantization of luma channel, default 6 [requires -z]

**-C <val>**  quantization of chroma channel, default 6 [requires -z]

**-A <val>**  quantization of alpha channel, default 5 [requires -z]

**-N <val>**  quantization of normals, default 10 [requires -z]

**-T <val>**  quantization of textures, default 0.25 [requires -z]

**-Q <val>**  quantization as a factor of error, default 0.1 [requires -z]
