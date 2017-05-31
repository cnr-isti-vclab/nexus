# Nexus

Nexus is a c++/javascript library for creation and visualization of a batched multiresolution mesh structure.

[Nexus](http://vcg.isti.cnr.it/nexus/) by [Visual Computing Laboratory](http://vcg.isti.cnr.it) - ISTI - CNR

Contact me @ federico.ponchio@isti.cnr.it 

### Main features

* Multiresolution
* Large models
* Textures or color per vertex
* Streaming
* Compression
* WebGL

### Create the .nxs model

Use [nxsbuild](doc/nxsbuild.md) to create a multiresolution nexus model (.nxs) out of your mesh (.ply):

	$ nxsbuild bunny.ply

The result will be bunny.nxs. For large files this may take quite some time. See the [man](doc/nxsbuild.md) page for all the options, supported input files etc.


### Simplify and/or compress

The model can be compressed, saving aroung 90% of the size. This is most useful for streaming applications: 

	$ nxsedit bunny.nxs -z -o bunny.nxz

Detailed information about the compression parameters can be found in the [man](doc/nxsedit.md) page.

You can get some statistics using [nxsedit](doc/nxsedit.md) on the created model: bounding sphere, list of patches created along with their error using nxsedit:

	$ nxsedit bunny.nxs -i -n


The .nxs file can be simplified: you can prune the lowest level of the resolution using the -l option.


### Inspect your model.

[Nxsview](doc/nxsview.md) is a simple program for inspecting a .nxs file:

	$ nxsview bunny.nxs 

You can tune various parameters through the interface, but be sure to read the available options in the [man](doc/nxsview.md) page.


### WebGL

The easiest way to publish the model on the web is to use [3DHOP](http://vcg.isti.cnr.it/3dhop/) interface.
Alternatively you can use Threejs: there is a minimal example in the html directory of the repository.

It is strongly recommended to use compression for the models (nxsedit -z).

### Library

The visualization algorithm can be easily (?) used as library inside your engine, both in C++ or in javascript,
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



