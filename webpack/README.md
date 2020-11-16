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
You can change the material of the NXS either by replacing the material or by changing a value (the color for exmple)
and set material.needsUpdate = true. 

## Roadmap

Cache: at the moment NXS creates a cache per model, we shuold have a global one per context (as it was), but the options to have .
Nexus global object with cache management.

Instances: 
1) highest error should be used in cache (now we refer to mesh)
2) double requests for the same node (be sure to check if it's already requested)

GPU resource transfer metering: better control of the amount of geometry and texture sent to the GPU per frame.

Fps: cache should keep track of timing and fps.

Prefetch: control prefetch amouont in MB (unlimited in case) and add onProgress event.

Library: make the code an npm library
https://webpack.js.org/guides/author-libraries/
