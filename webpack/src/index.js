//npx webpack

import {
    Vector3,
    Color,
    Fog,
    AmbientLight,
    DirectionalLight,
    PerspectiveCamera,
    WebGLRenderer,
    Scene,
    Clock,


    BoxBufferGeometry,
    Mesh,
    MeshBasicMaterial,
} from 'three'

import { TrackballControls } from 'three/examples/jsm/controls/TrackballControls.js';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';

import { NXS} from './NXS.js'
import { Monitor } from './monitor.js'


let container = document.getElementById( 'scene');
console.log("canvas", container);

let camera = new PerspectiveCamera( 30, container.innerWidth / container.innerHeight, 0.1, 100 );
camera.position.set(0, 0, 10);


let controls_options = {
    rotateSpeed: 0.5,
    zoomSpeed: 4,
    panSpeed: 0.8,
    noZoom: false,
    noPan: false,
    enableDamping: true,
    staticMoving: true,
    dynamicDampingFactor: 0.3,
}
let controls = new OrbitControls(camera, container );
for( const [key, value] of Object.entries(controls_options))
    controls[key] = value; 

controls.addEventListener( 'change', function() { redraw = true; } );



let scene = new Scene();
scene.background = new Color( 0xaaaaff );
scene.fog = new Fog( 0x050505, 2000, 3500 );
scene.add( new AmbientLight( 0x444444 ) );

var light1 = new DirectionalLight( 0xffffff, 1.0 );
light1.position.set( 1, 1, -1 );
scene.add( light1 );

var light2 = new DirectionalLight( 0xffffff, 1.0 );
light2.position.set( -1, -1, 1 );
scene.add( light2 ); 

var renderer = new WebGLRenderer( { antialias: false } );
renderer.setClearColor( scene.fog.color );
renderer.setPixelRatio( window.devicePixelRatio );

container.append(renderer.domElement);


function onNexusLoad(model) {
    const p = model.boundingSphere.center.negate();
    const s   = 1/model.boundingSphere.radius;

	nexus.position.set(p.x*s, p.y*s, p.z*s);
	nexus.scale.set(s, s, s); 
	redraw = true;
}

var url = "models/gargo.nxz"; 

let nexus = new NXS(url, onNexusLoad, () => { redraw = true; });
//nexus.material = new MeshBasicMaterial( { color: 0xff0000 } );
let monitor = new Monitor(nexus.cache);
scene.add(nexus);

new ResizeObserver(onWindowResize).observe(container);

function onWindowResize() {
    camera.aspect = container.clientWidth / container.clientHeight;
	camera.updateProjectionMatrix();

	renderer.setSize( container.clientWidth, container.clientHeight );

	controls.update();
	redraw = true;
}

const clock = new Clock();
var redraw = true;

renderer.setAnimationLoop(()=> {
    controls.update();

    const delta = clock.getDelta()
    
	if(redraw) {
        nexus.cache.beginFrame(30);
		renderer.render( scene, camera );
        redraw = false;
        nexus.cache.endFrame();
    }
})

onWindowResize();
