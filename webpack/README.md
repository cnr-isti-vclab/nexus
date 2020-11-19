## Install

Install threejs:
```bash
npm install -save three
```

Install webpack, (optional):
```bash
npm install -save webpack webpack-cli
```
Build the example in webpack:
```bash
npx webpack --mode development
```

If you don't want to usa another http server:
```bash
npm install webpack-dev-server --save-dev
```
Finally add the following lines to package.json
```json
"scripts": {
  "start:dev": "webpack-dev-server"
}
```

and run the server:
```bash
npm run start:dev
```

## Docs

# Material

NXS material is actually a template material that is used for each patch of the nexus, you cannot set the map properties, 
which will be overwritten by the patches own textures.
You can change the material of the NXS either by replacing the material or by changing a value
(the color for exmple). If you want to disable texture rendering set the parameter 'map' to false,
and set material.needsUpdate = true. 

# FPS:
The Cache class can throttle the amount of geometry rendered trying to mantain the fps above a certain target (minFps), 
reducing the targetError. You can set this property through the NXS.cache.minFps property. (default 30).

# Instances:
You can render multiple instances of the same mesh, just using the reference to an existing NXS
instead of the url in the constructor. Cache will be shared, balancing the quality among all the instances.


## Roadmap

Cache: at the moment NXS creates a cache per model, we shuold have a global one per context (as it was), but the options to have .
Nexus global object with cache management.

TODO: check minFps actually works.

GPU resource transfer metering: better control of the amount of geometry and texture sent to the GPU per frame.
Prefetch: control prefetch amouont in MB (unlimited in case) and add onProgress event.
use blocked for prefetch based on size, number of nodes.
onProgress when some node is added or removed.

Library: make the code an npm library
https://webpack.js.org/guides/author-libraries/
