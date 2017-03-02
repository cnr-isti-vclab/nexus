#Nxsedit

	nxsedit [OPTIONS] [NEXUS INPUT]

Nxsedit can be used for a number of post-processing operations over a nxs file:
compression, adding color from projected photos, pruning, etc.

All operations generate a new nexus leaving the old one untouched.

###Options

**-o FILE**: filename of the nexus output file, .nxs is added automatically. If not specified no output file is created.

**-i**: prints info about the nexus: number of patches, dag etc. The number of
      triangles and vertices includes all the resolutions, you can assume the
      highest resolution level holds half the total amount.

**-n**: prints info about nodes

**-q**: prints info about payches

**-d**: prints info about dag

**-c**: performs various checks

**-p FILE**: filename of the ply output file

**-s M**: extract a subset of the nxs of a given size (in MB), pruning lower error nodes. (requires -E option)

**-e E**: extract a subset of the nxs pruning nodes with error below the given value (-i -n options can be used to inspect node errors).

**-t T**: extract a subset of the nxs of a given triangle count, pruning lower error nodes. 

**-l**: prune the last level of nodes from the nexus (thus approximately halving the
     size and the triangle count)

**-z**: Applies a (somewhat lossy) compression algorithm to each patch. You can tune quantization parameters using the following options.

**-v S**: absolute side of compression quantization grid

**-V B**: number of bits in vertex coordinate when compressing

**-Y B**: quantization of luma channel (default 6 bits)

**-C B**: quantization of chroma channel (default 6 bits)

**-A B**: quantization of alpha channel (default bits)

**-N B**: quantization of normals (default 10 bits)

**-T Q**: quantization of textures coordinates (default 0.25 pixel)

**-Q Q**: coordinates quantization grid as a fraction of smallest node error (default 0.1)

**-E METHOD**: recompute errors (average, quadratic, logarithmic)

**-m MATRIX**: multiply by matrix44 in format a:b:c...

**-M MATRIX**: multiply by inverse of matrix44 in format a:b:c...


