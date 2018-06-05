# Nxscompress

Nxscompress can be used to compress the .nxs multiresolution model into a .nxz multiresolution compressed model:

	nxscompress [NEXUS INPUT FILE] [OPTIONS]

All operations generate a new nexus leaving the old one untouched.

### Options

**-o <val>**  filename of the nexus output file
**-Z <val>**  <val> chooses among compression libs [corto, meco], default corto
**-v <val>**  absolute side of quantization grid, default 0.0 (uses quantization factor, instead)
**-V <val>**  number of bits in vertex coordinates when compressing default, 0 (uses quantization factor, instead)
**-Q <val>**  quantization as a factor of error, default 0.1
**-Y <val>**  quantization of luma channel, default 6
**-C <val>**  quantization of chroma channel, default 6
**-A <val>**  quantization of alpha channel, default 5
**-N <val>**  quantization of normals, default 10
**-T <val>**  quantization of textures, default 0.25
