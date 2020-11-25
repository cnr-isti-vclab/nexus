//npx webpack

import {
    Vector2,
    Euler,
    Color,
    Fog,
    AmbientLight,
    DirectionalLight,
    PerspectiveCamera,
    Raycaster,
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

import { Nexus3D} from './Nexus3D.js'
import { Monitor } from './Monitor.js'


let container = document.getElementById( 'scene');

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


function onNexusLoad(nexus) {
    const p = nexus.boundingSphere.center.negate();
    const s   = 1/nexus.boundingSphere.radius;

    //nexus.rotateX(-3.1415/2);
	nexus.position.set(p.x*s, p.y*s, p.z*s);
	nexus.scale.set(s, s, s); 
	redraw = true;
}

var url = "models/gargo.nxz"; 

//onUpdate parameter here is used to trigger a redraw
let nexus = new Nexus3D(url, onNexusLoad, () => { redraw = true; }, renderer);

//create a second instance and position it.
//let nexus1 = new Nexus3D(nexus, (m) => { onNexusLoad(m); m.position.x += 2 }, () => { redraw = true; }, renderer);

//material can be changed replacing the material or modifying it.
//nexus.material = new MeshBasicMaterial( { color: 0xff0000 } );
//nexus.material.color = new Color(1, 0, 0);

let monitor = new Monitor(nexus.cache);
scene.add(nexus);
//scene.add(nexus1);


var mouse = new Vector2();

function onMouseMove( event ) {
    event.preventDefault();
	mouse.x = ( event.clientX / container.offsetWidth ) * 2 - 1;
    mouse.y = - ( event.clientY / container.offsetHeight ) * 2 + 1;
}

document.addEventListener( 'mousemove', onMouseMove, false );



new ResizeObserver(onWindowResize).observe(container);

function onWindowResize() {
    camera.aspect = container.clientWidth / container.clientHeight;
	camera.updateProjectionMatrix();

	renderer.setSize( container.clientWidth, container.clientHeight );

	redraw = true;
}

const clock = new Clock();
var redraw = true;

renderer.setAnimationLoop(()=> {
    const delta = clock.getDelta()
    controls.update(delta);
    
	if(redraw) {
	//during rendering it might be apparent we need another render pass, set it to false BEFORE render
        redraw = false; 


        var raycaster = new Raycaster();
	    raycaster.setFromCamera( mouse, camera );

	    var intersections = raycaster.intersectObjects( [nexus], true );
	    if(intersections.length) {
		    nexus.material.color =  new Color(1, 0, 0);
        } else {
            nexus.material.color =  new Color(1, 1, 1);
        }
 	    nexus.material.needsUpdate = true;
        redraw = true;

        nexus.cache.beginFrame(30);
		renderer.render( scene, camera );
        nexus.cache.endFrame();
    }
})

onWindowResize();
