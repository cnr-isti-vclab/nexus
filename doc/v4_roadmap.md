# Nexus v4 Rewrite Roadmap

## Building

*   **Algorithm**: Change from kdtree triangle soup streaming to out-of-core streaming indexed mesh processing with wedge table.
*   **Clustering**: 2-level clustering (macro and micro nodes). Macro is basically the current nexus level, we add a finer granularity.
*   **Textures**: Texture reparametrization per macro-node. Texture and geometry size in bins for easier GPU memory management.
*   **Materials**: Grouped by BRDF. Same BRDF will get textures merged in a single atlas (number and type of textures).

## Rendering

*   **Targets**: WebGL and WebGPU.
*   **Buffers**: Single GPU vertex/index/texture buffer minimize number of draw calls.
*   **Control**: CPU macro DAG traversal controls what's in GPU (parent-children consistency).
*   **WebGL Strategy**: Traversal in CPU and draw calls.
*   **WebGPU Strategy**: Traversal in GPU at the micro level selects nodes, determine the index interval from the edges of the micro-dag, compact the indexes in a new index buffer, single draw call (per material).

## Streaming Processing & Bilevel Clustering

1.  **Micro-nodes**: Make a subdivision in small (128/256) micro nodes of the model. Build graph of adjacencies.
2.  **Macro-nodes Level A**: Subdivision in macro-nodes composed of micro-nodes.
3.  **Macro-nodes Level B**: Larger subdivision in macro-nodes (double the size) to minimize boundaries in common.
4.  **Join-Simplify-Merge**:
    *   For each level B macro-node:
        *   Create a subdivision in micronodes with double triangles (level B micronodes).
        *   Simplify the mesh respecting the level B micronodes boundaries.
        *   Save the level B micronodes taking into account dependencies and index intervals.
        *   Save level B macronode.

**Multiresolution on Macrolevels**: Assemble the micronodes in a specific order such to have 1 index interval per dependency AND these boundaries will be simplified in the next level.

**Creation Steps**:
1.  Create the graph of node adjacencies with node weight (number of triangles) and edge weight (shared edge count).
2.  Greedy growth of multiple seeds (take distance from centroid into account, but also minimize boundary in common across levels).
3.  KL refinement.
