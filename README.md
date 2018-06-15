# Nexus

Nexus is a c++/javascript library for creation and visualization of a batched multiresolution 3D model structure.

[Nexus](http://vcg.isti.cnr.it/nexus/) by [Visual Computing Laboratory](http://vcg.isti.cnr.it) - ISTI - CNR

Contact me @ federico.ponchio@isti.cnr.it 

### Main features

* Multiresolution
* Large models
* Textures or color per vertex
* Streaming
* Compression
* WebGL

### Basic usage

Starting from a 3D model (.ply), drag and drop it on the **nxsbuild** executable, and it will be converted into a multiresolution nexus model (.nxs). Drag the multiresolution nexus model (.nxs) onto the **nxscompress** executable to compress it, and the result will be a compressed multiresolution nexus model (.nxz). 

gargo.ply --> **nxsbuild** --> gargo.nxs --> **nxscompress.exe** --> gargo.nxz

Drag and drop either .nxs or .nxz files on **nxsview** to interactively inspect the generated 3D multiresolution model.

-----------------------------------------------------------------------------------------

### Create the .nxs model

Use [nxsbuild](doc/nxsbuild.md) to create a multiresolution nexus model (.nxs) out of your 3D model (.ply):

	$ nxsbuild gargo.ply

The result will be gargo.nxs. For large files this may take quite some time. See the [man](doc/nxsbuild.md) page for all the options, supported input files etc.

### Compress the multiresolution model

The model can be compressed, saving aroung 90% of the size. This is most useful for streaming applications:

	$ nxcompress gargo.nxs

The result will be gargo.nxz.
Detailed information about the compression parameters can be found in the [man](doc/nxcompress.md) page.


### Edit, Info and Simplify

[Nxsedit](doc/nxsedit.md) can be used for many editing operations on the multiresolution model.
For instance, you can get some statistics on the created model (bounding sphere, list of patches, etc.):

	$ nxsedit gargo.nxs -i

Or also, you can simplify the .nxs file (pruning the lowest level of the multiresolution tree):

	$ nxsedit gargo.nxs -l -o simplified_gargo.nxs

Detailed information about the editing parameters can be found in the [man](doc/nxedit.md) page.

### Inspect your model.

[Nxsview](doc/nxsview.md) is a simple program for inspecting a .nxs file:

	$ nxsview gargo.nxs 

You can tune various parameters through the interface, but be sure to read the available options in the [man](doc/nxsview.md) page.


### WebGL

The easiest way to publish the model on the web is to use [3DHOP](http://3dhop.net) interface.
Alternatively you can use Three.js: there is a minimal example in the HTML directory of the GitHub [Nexus repository](https://github.com/cnr-isti-vclab/nexus).
It is strongly recommended to use compression for the models (nxscompress).


### Library

The visualization algorithm can be easily used as library inside your engine, both in C++ or in JavaScript,
basically the algorithm job is to send geometry to the GPU.


### Dependencies and Licenses

All Nexus software is free and released under the GPL license (it depends on Qt and VCG lib).


### Publications

[Multiresolution and fast decompression for optimal web-based rendering](http://vcg.isti.cnr.it/Publications/2016/PD16/FastDec_Ponchio.pdf)
Federico Ponchio, Matteo Dellepiane
Graphical Models, Volume 88, pp. 1-11, November 2016

[Fast decompression for web-based view-dependent 3D rendering](http://vcg.isti.cnr.it/Publications/2015/PD15/Ponchio_Compressed.pdf)
Federico Ponchio, Matteo Dellepiane
Web3D 2015. Proceedings of the 20th International Conference on 3D Web Technology , pp. 199-207, June 2015

[Multiresolution structures for interactive visualization of very large 3D datasets](http://vcg.isti.cnr.it/~ponchio/download/ponchio_phd.pdf)
Federico Ponchio
Phd Thesis

[Interactive Rendering of Dynamic Geometry](http://vcg.isti.cnr.it/Publications/2008/PH08/dynamic.pdf)
F. Ponchio, K. Hormann
IEEE Transaction on Visualization and Computer Graphics, Volume 14, Number 4, pp. 914-925, July 2008

[Batched Multi Triangulation](http://vcg.isti.cnr.it/Publications/2005/CGGMPS05/BatchedMT_Vis05.pdf)
Paolo Cignoni, Fabio Ganovelli, Enrico Gobbetti, Fabio Marton, Federico Ponchio, Roberto Scopigno
Proceedings IEEE Visualization, pp. 207-214, October 2005

[Adaptive TetraPuzzles: Efficient Out-of-Core Construction and Visualization of Gigantic Multiresolution Polygonal Models](http://vcg.isti.cnr.it/publications/papers/vbdam_sig04.pdf)
P. Cignoni, F. Ganovelli, E. Gobbetti, F. Marton, F. Ponchio, R. Scopigno
ACM Trans. on Graphics, vol. 23(3), pp. 796-803, August 2004 (Siggraph '04)

[BDAM: Batched Dynamic Adaptive Meshes for High Performance Terrain Visualization](http://vcg.isti.cnr.it/publications/papers/bdam.pdf)
P.Cignoni, F.Ganovelli, E. Gobbetti, F.Marton, F. Ponchio, R. Scopigno
Computer Graphics Forum, 22(3), pp. 505-514, September 2003


### Support and thanks

Thanks, support: VCL Informatic department in TU Clausthal, 3D-COFORM. Also to Kai Hormann for having me write the thesis :)



