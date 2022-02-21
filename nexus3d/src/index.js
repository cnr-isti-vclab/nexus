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
    

    Mesh,
    BoxGeometry,
    MeshPhongMaterial,
    BasicShadowMap
    //BoxBufferGeometry,
    
    //MeshBasicMaterial,
} from 'three'

import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import * as Nexus3D from './Nexus3D.js'
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
light1.position.set( 2, 1, -0.5 );

light1.castShadow = true;
light1.shadow.camera.near = 1;
light1.shadow.camera.far = 10;
light1.shadow.camera.right = 2;
light1.shadow.camera.left = - 2;
light1.shadow.camera.top	= 2;
light1.shadow.camera.bottom = - 2;
light1.shadow.mapSize.width = 1024;
light1.shadow.mapSize.height = 1024;


scene.add( light1 );

var light2 = new DirectionalLight( 0xffffff, 1.0 );
light2.position.set( -1, -1, 1 );
//scene.add( light2 ); 

var renderer = new WebGLRenderer( { antialias: false } );
renderer.setClearColor( scene.fog.color );
renderer.setPixelRatio( window.devicePixelRatio );
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = BasicShadowMap;

container.append(renderer.domElement);


function onNexusLoad(nexus) {
	const p   =   nexus.boundingSphere.center.negate();
	const s   = 1/nexus.boundingSphere.radius;
	//nexus.rotateX(-3.1415/2);

	if(nexus == nexus1) {
		nexus.position.set(p.x*s + 1, p.y*s, p.z*s);
	} else
		nexus.position.set(p.x*s - 1, p.y*s, p.z*s);

	nexus.scale.set(s, s, s); 
	redraw = true;
}

var url = "models/gargo.nxz"; 

//onUpdate parameter here is used to trigger a redraw
let nexus1 = new Nexus3D.Nexus3D(url, renderer, { onLoad: onNexusLoad, onUpdate: () => { redraw = true; }} );
//nexus1.castShadow = true;
nexus1.receiveShadow = true;

//create a second instance and position it.
let nexus2 = new Nexus3D.Nexus3D(url, renderer, { onLoad: onNexusLoad, onUpdate: () => { redraw = true; }} );
nexus2.castShadow = true;
nexus2.receiveShadow = true;



//material can be changed replacing the material or modifying it.
//nexus.material = new MeshBasicMaterial( { color: 0xff0000 } );
//nexus.material.color = new Color(1, 0, 0);

let monitor = new Monitor(Nexus3D.Cache, nexus2);
scene.add(nexus1);
scene.add(nexus2);

nexus1.castShadow = true;


let geometry = new BoxGeometry( 10, 0.15, 10 );
let material = new MeshPhongMaterial( {
    color: 0xa0adaf,
    shininess: 150,
    specular: 0x111111
} );

const ground = new Mesh( geometry, material );
ground.scale.multiplyScalar( 3 );
ground.castShadow = false;
ground.receiveShadow = true;
ground.position.set(0, -1, 0);
scene.add( ground );



window.addNexus = () => { 
	nexus2 = new Nexus3D.Nexus3D(url, renderer, { onLoad: onNexusLoad, onUpdate: () => { redraw = true; }} );
	scene.add(nexus2);
	redraw = true;
};
window.removeNexus = () => { 
	scene.remove(nexus2);
	nexus2.dispose();
	nexus2 = null;
	redraw = true;
};

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
        redraw = true; 


        var raycaster = new Raycaster();
	    raycaster.setFromCamera( mouse, camera );

	    var intersections = raycaster.intersectObjects( [nexus1], true );
	    if(intersections.length) {
		    nexus1.material.color =  new Color(1, 0, 0);
	 	    nexus1.material.needsUpdate = true;
        } else {
            nexus1.material.color =  new Color(1, 1, 1);
        }
		renderer.render( scene, camera );
    }
})

onWindowResize();
