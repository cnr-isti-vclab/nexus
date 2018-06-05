# Nxsview

Nxsview is a demo progam for nexus models visualization. It exposes the parameters of the visualization algorithm for testing purpouses:

	nxsview [OPTIONS] [NEXUS INPUT FILE]

### Options

**-d TRIANGLES**: max number of triangles

**-e ERROR**: target projected error (in pixels) for the rendering algorithm (default 2). Lower values generate higher quality images but might result in slower fps

**-m RAM**: max ram used (in Mb)

**-g RAM**: max video ram used (in Mb)

**-c ITEMS**: max number of items in cache

**-v DEG**: field of view in degrees

**-h HEIGHT**: height of the window

**-w WIDTH**: width of the window

**-s**: fullscreen mode

**-b**: backface culling

**-o**: occlusion culling

**-S**: do not use another thread for textures

**-f FPS**: target frames per second

**-i N**: number of instances

**-a**: distribute models ina a grid

**-p PREFETCHED**: amount of node prefetching in prioritizer
