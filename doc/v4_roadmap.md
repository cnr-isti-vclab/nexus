# Nexus v4 Rewrite Roadmap

## Building

*   **Spatial clustering**: Change from kdtree triangle soup streaming to out-of-core streaming indexed mesh processing with wedge table.
*   **2 levels DAG**: 2-level clustering (macro and micro nodes). Macro is basically the current nexus level, we add a finer granularity.
*   **Textures**: Texture reparametrization per macro-node. Texture and geometry size in bins for easier GPU memory management.
*   **Materials**: Single object single brdf, just with the minimal common requirements (don't store textures unused)

## Rendering

*   **Targets**: WebGL and WebGPU.
*   **Buffers**: Single GPU vertex/index/texture buffer minimize number of draw calls.
*   **Control**: CPU macro DAG traversal controls what's in GPU (parent-children consistency).
*   **WebGL Strategy**: Traversal in CPU and draw calls.
*   **WebGPU Strategy**: Traversal in GPU at the micro level selects nodes, determine the index interval from the edges of the micro-dag, compact the indexes in a new index buffer, single draw call (per material).

## Streaming Processing 

We use a memory mapped mesh to store and proces the initial mesh.

* read the mesh and save into the MappedMesh list of files.
* spatial sort of the mesh
    * sort positions (merge sort + morton) and unigy.
    * remap and sort wedges
    * sort normals and textures
    * remap triangles, unify wedges
    * sort triangles
* compute adjacency (TODO: use merge sort and mapped halfedges, if needed)
* recompute normals

## Bilevel Clustering 

# Current monolevel construction:

1.  **Micro-nodes**: 
* Make a subdivision in small (128/256) micro nodes of the model using METIS.
* reorder the triangles, compute bounding volume for micronodes
* split micronodes into microclusters (again metis per micronode)
* reorder by microclusters.
* IF TEX: reparametrize initial nodes for each:
    * merge the clusters
    * reparametrize the node
    * rasterize the node in tex space to create new texture
    * update wedges and texcvoords
* TODO: the tex parametrization  needs to happena at macronode level

1. ** Hierarchy **
* copy vertices (do we need it?)
* compute texel ratio see specific discussion
* For each node:
    * merge micronodes
    * simplify
    * split (less parts, minimize edge and try to erase old boundaries)
* recompute all adjaceccies and normals
* from the clusters create the new micronodes (metis)
* IF TEX: reparaemetrize the micronode TODO this must happen at macronodelevel

# Modifications for the second level

1. After the initial build of the micronodes 
    * group them in macronodes,  
    * THEN reparametrize
    * sort clusters

2. After the second level micronoes creation 
    * compute the new macronodes minimizing edge and  preservation of boundary
    * now reparametrize
    * sort clusters

In all of this we need to keep the list of micronodes for each macronode adn the resulting DAG.


## Export to nxs

# Current

# Bilevel

## 