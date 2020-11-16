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





## Roadmap

Material: due to the group mechanic in threejs groups, the materia internally is an array.
Can be shared among nodes only if no maps are set, otherwise the material is cloned.
If the 'template' material is changed in case of textures we need to upcate all the node materials.
Material template without texture used on models with textures: map = false instead of null?

Debug: it's nice to render each patch with a different color, a sort of debug mode, either by original error, 
render error, level (complicated), or just random. We need to store the previous material template and restore it when
enabling/disabling node debug mode.

Cache: at the moment NXS creates a cache per model, we shuold have a global one per context (as it was), but the options to have .
Nexus global object with cache management.

Instances: 
1) highest error should be used in cache (now we refer to mesh)
2) double requests for the same node (be sure to check if it's already requested)

GPU resource transfer metering: better control of the amount of geometry and texture sent to the GPU per frame.

Fps: cache should keep track of timing and fps.

Library: make the code an npm library
https://webpack.js.org/guides/author-libraries/
