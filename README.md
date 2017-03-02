# Nexus

Nexus is a c++/javascript library for creation and visualization of a batched multiresolution mesh structure.

Main features:

* Multiresolution
* Large models
* Textures or color per vertex
* Streaming
* Compression
* WebGL

###Create the .nxs model

Use [nxsbuild](doc/nxsbuild.md) to create a multiresolution nexus model (.nxs) out of your mesh (.ply):

	$ nxsbuild bunny.ply

The result will be bunny.nxs. For large files this may take quite some time. See the [man](doc/nxsbuild.md) page for all the options, supported input files etc.


###Simplify and/or compress

The model can be compressed, saving aroung 90% of the size. This is most useful for streaming applications. 

	$ nxsedit bunny.nxs -z -o bunny.nxz

Detailed information about the compression parameters can be found in the [man](doc/nxsedit.md) page.

You can get some statistics using [nxsedit](doc/nxsedit.md) on the created model: bounding sphere, list of patches created along with their error using nxsedit:

	$ nxsedit bunny.nxs -i -n


The .nxs file can be simplified: you can prune the lowest level of the resolution using the -l option.


###Inspect your model.

[Nxsview](doc/nxsview.md) is a simple program for inspecting a .nxs file:

	$ nxsview bunny.nxs 

You can tune various parameters through the interface, but be sure to read the available options in the [man](doc/nxsview.md) page.


###WebGL

The easiest way to publish the model on the web is to use [3DHOP](http://vcg.isti.cnr.it/3dhop/) interface.
Alternatively you can use Threejs: there is a minimal example in the html directory of the repository.

It is strongly recommended to use compression for the models (nxsedit -z).

###Library

The visualization algorithm can be easily(?) used as library inside your engine, both in C++ or in javascript,
basically the algorithm job is to send geometry to the GPU.

###Dependencies and Licenses

All Nexus software is free and released under the GPL license (it depends on Qt and VCG lib).


### Publications

[Fast decompression for web-based view-dependent 3D rendering](http://vcg.isti.cnr.it/Publications/2015/PD15/)<br/>
Federico Ponchio, Matteo Dellepiane<br/>
Web3D 2015. Proceedings of the 20th International Conference on 3D Web Technology , page 199-207 - 2015

[Multiresolution structures for interactive visualization of very large 3D datasets](http://vcg.isti.cnr.it/~ponchio/download/ponchio_phd.pdf) (20MB)<br/>
Federico Ponchio<br/>
Phd Thesis

[Batched Multi Triangulation](http://vcg.isti.cnr.it/Publications/2005/CGGMPS05/BatchedMT_Vis05.pdf)<br/>
Paolo Cignoni, Fabio Ganovelli, Enrico Gobbetti, Fabio Marton, Federico Ponchio, Roberto Scopigno<br/>
Proceedings IEEE Visualization, page 207-214 - October 2005

[Interactive Rendering of Dynamic Geometry](http://vcg.isti.cnr.it/Publications/2008/PH08/dynamic.pdf)<br/>
F. Ponchio, K. Hormann<br/>
IEEE Transaction on Visualization and Computer Graphics, Volume 14, Number 4, page 914--925 - Jul 2008

[Adaptive TetraPuzzles: Efficient Out-of-Core Construction and Visualization of Gigantic Multiresolution Polygonal Models](http://vcg.isti.cnr.it/publications/papers/vbdam_sig04.pdf)<br/>
P. Cignoni, F. Ganovelli, E. Gobbetti, F. Marton, F. Ponchio, R. Scopigno<br/>
ACM Trans. on Graphics, vol. 23(3), August 2004, pp. 796-803, (Siggraph '04)

[BDAM: Batched Dynamic Adaptive Meshes for High Performance Terrain Visualization](http://vcg.isti.cnr.it/publications/papers/bdam.pdf)<br/>
P.Cignoni, F.Ganovelli, E. Gobbetti, F.Marton, F. Ponchio, R. Scopigno<br/>
Computer Graphics Forum, 22(3), Sept. 2003, pp.505-514.

###Support and thanks

Thanks, support: VCL Informatic department in TU Clausthal, 3D-COFORM. Also to Kai Hormann for having me write the thesis :)



