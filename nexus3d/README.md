# Nexus3D

Streaming 3D on the web (three.js supported).

## Install

Install three.js (npm will save to `dependencies` automatically):

```bash
npm install three
```

If you use webpack for local development:

```bash
npm install --save-dev webpack webpack-cli webpack-dev-server
```

Build the example with webpack:

```bash
npx webpack --mode development
```

Run the dev server:

```bash
npm run start
```

## Getting started (minimal)

**Important:** Nexus3D expects three.js to be available. You need to include three.js in your project either via npm or a CDN.

```js
import * as THREE from 'three';
import { Nexus3D } from './build/nexus3D.js';

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.1, 1000);

const url = 'https://example.com/models/my.nxs';
const n = new Nexus3D(url, renderer, {
  material: new THREE.MeshStandardMaterial(),
  onLoad: (mesh) => { scene.add(mesh); },
  onProgress: (mesh, available, total) => { console.log('progress', available, total); }
});

function animate() {
  requestAnimationFrame(animate);
  renderer.render(scene, camera);
}
animate();
```

## API (brief)

- Constructor: `new Nexus3D(urlOrNxsObject, renderer, options = {})`
  - `urlOrNxsObject`: string URL to the NXS file or an already-loaded NXS object
  - `renderer`: a `THREE.WebGLRenderer` instance (required)
  - `options`:
    - `material`: optional `THREE.Material` instance to use as the template
    - `onLoad(mesh)`: optional function called when root metadata is ready
    - `onUpdate(mesh)`: optional function called when node availability changes
    - `onProgress(mesh, availableNodes, nodesCount)`: progress callback

- Cache: a shared `Cache` instance is used by default (see `src/Cache.js`). You can tune behavior via `mesh.cache.minFps` and similar properties.

## Docs

### Material

NXS material is actually a template material that is used for each patch of the nexus, you cannot set the map properties, which will be overwritten by the patches own textures.

You can change the material of the NXS either by replacing the material or by changing a value (the color for example). If you want to disable texture rendering set the parameter 'map' to false, and set `material.needsUpdate = true`.

### FPS

The Cache class can throttle the amount of geometry rendered trying to maintain the fps above a certain target (minFps), reducing the targetError. You can set this property through the `NXS.cache.minFps` property (default 30).

### Instances

You can render multiple instances of the same mesh, just using the reference to an existing NXS instead of the url in the constructor. Cache will be shared, balancing the quality among all the instances.

## Notes

- This library is compatible with three.js v0.181.x (see `package.json`).

## Roadmap / TODO

- Make cache global per rendering context instead of per-model.
- GPU transfer metering and prefetch control.
- Improve examples and add automated tests.
- Add CI to build and test on push.

## Contributing

Please open issues or pull requests on the repository. If you change three.js versions, please note compatibility in the README.
