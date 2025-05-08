import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import { TransformControls } from 'three/addons/controls/TransformControls.js';
import {GUI} from 'three/addons/libs/lil-gui.module.min.js';
import createModule from "../wasm/main.js";

const particle_count = 30;
const box_size = 4;

const pixel_count = 8;
const pixel_gap_total = 0.3;
const pixel_size = box_size * 2 * (1 - pixel_gap_total) / pixel_count;
const pixel_gap_size = box_size * 2 * pixel_gap_total / (pixel_count - 1);
const face_dot_size = 0.05;
const pixel_thickness = 0.01;

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera( 75, window.innerWidth / window.innerHeight, 0.1, 1000 );
camera.position.z = 20;


const renderer = new THREE.WebGLRenderer();
const canvas = renderer.domElement;
renderer.setSize( window.innerWidth, window.innerHeight );
document.body.appendChild( renderer.domElement );


// Create a wireframe material
const wireframeMaterial = new THREE.MeshBasicMaterial({
  color: 0xffffff,
  wireframe: true,
});

let control_mode = "orbit"

const axesHelper = new THREE.AxesHelper(5); // size 5 units

// Add it to the scene
scene.add(axesHelper);

const gui = new GUI();

const settings = {
	r_force: 50.0,
	gravity: 20
}
gui.add(settings, 'r_force', 0, 200).name('R Force');
gui.add(settings, 'gravity', 0, 100).name('Gravity');

const cube_group = new THREE.Group();

const particles = []
const sphereGeometry = new THREE.SphereGeometry(1, 8, 8);
for (let i = 0; i < particle_count; i++)
{
	const sphere = new THREE.Mesh(sphereGeometry, wireframeMaterial);
	cube_group.add(sphere);
	particles.push(sphere);
}

const geometry = new THREE.BoxGeometry(box_size*2,box_size*2,box_size*2)
const material = new THREE.MeshPhongMaterial({color: '#8AC'});
const cube = new THREE.Mesh( geometry,wireframeMaterial );
cube_group.add( cube );
// scene.add( cube );

const line_material = new THREE.LineBasicMaterial({ color: 0x0000ff });
const line_geometry = new THREE.BufferGeometry();
const line_vertices = [];
for (let i = 0; i < particle_count * 6; i++)
	line_vertices[i] = 0;

line_geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(line_vertices), 3));

const lines = new THREE.LineSegments(line_geometry, line_material);
cube_group.add(lines);

scene.add(cube_group)

const orbit = new OrbitControls(camera, renderer.domElement );
orbit.update();
orbit.target.set(0, 0, 0);

const t_controls = new TransformControls(camera, canvas)
t_controls.addEventListener( 'dragging-changed', function ( event ) {
	orbit.enabled = ! event.value;
} );

const gizmo = t_controls.getHelper();
scene.add( gizmo );

const localDown = new THREE.Vector3(0, -1, 0);

function set_control_mode(new_mode) {
	control_mode = new_mode;
	console.log(t_controls);

	if (control_mode == "orbit")
	{
		t_controls.detach(cube_group);
	} else if (control_mode == "rotate") {
		t_controls.setMode("rotate")
		console.log(control_mode);
		t_controls.attach(cube_group);
	} else if (control_mode == "transform") {
		t_controls.attach(cube_group);
	}
}

function pixel_grid_pos(i) {
	return i * (pixel_size + pixel_gap_size);	
} 


const pixel_geometry = new THREE.BoxGeometry( pixel_size, pixel_size, pixel_thickness); 
const face_marker_geometry = new THREE.BoxGeometry( face_dot_size * box_size, face_dot_size * box_size, pixel_thickness); 
const face_marker_material = new THREE.MeshBasicMaterial( { color: 0xff0000 } );

class LEDGrid
{
	constructor()
	{
		this.group = new THREE.Group();	
		this.pixels = [];
		
		this.face_marker = new THREE.Mesh( face_marker_geometry, face_marker_material );
		this.group.add(this.face_marker);
		this.face_marker.position.set(-box_size, -box_size, 0);

		for (let i = 0; i < 8; i++)
			for (let j = 0; j < 8; j++)
			{
				const pixel_material = new THREE.MeshBasicMaterial( {color: 0x00ff00} );
				const pixel = new THREE.Mesh( pixel_geometry, pixel_material ); 
				pixel.position.set(
					pixel_grid_pos(j) - box_size + pixel_size / 2,
					pixel_grid_pos(i) - box_size + pixel_size / 2,
					0);
				
				this.pixels.push(pixel);
				this.group.add(pixel);
			}


	}

	set_pixels(pixel_buffer, position)
	{
		for (let i = 0; i < 8; i++)
			for (let j = 0; j < 8; j++)
			this.pixels[i+j*8].material.color.set(0xFF * Math.min(pixel_buffer[position + (i+1) + (j+1)*10], 1));
	}
}

const FrontGrid = new LEDGrid();
FrontGrid.group.position.set(0, 0, box_size);
cube_group.add(FrontGrid.group);

const TopGrid = new LEDGrid();
TopGrid.group.position.set(0, box_size, 0);
TopGrid.group.rotateX(-Math.PI/2);
cube_group.add(TopGrid.group);

const RightGrid = new LEDGrid();
RightGrid.group.position.set(box_size, 0, 0);
RightGrid.group.rotateY(Math.PI/2);
cube_group.add(RightGrid.group);

const BottomGrid = new LEDGrid();
BottomGrid.group.position.set(0, -box_size, 0);
BottomGrid.group.rotateX(Math.PI/2);
cube_group.add(BottomGrid.group);

const BackGrid = new LEDGrid();
BackGrid.group.position.set(0, 0, -box_size);
BackGrid.group.rotateY(Math.PI);
cube_group.add(BackGrid.group);

const LeftGrid = new LEDGrid();
LeftGrid.group.position.set(-box_size, 0, 0);
LeftGrid.group.rotateY(-Math.PI/2);
cube_group.add(LeftGrid.group);





async function main() {
	const Module = await createModule();

	const initialize_particles = Module.cwrap('initialize_particles', 'void', []);
	initialize_particles();
	const tick_particles = Module.cwrap("tick_particles", 'void', ["number", "number", "number", "number", "number"])

	const pos_ptr = Module.ccall('get_position_buffer', 'number', [], []);
	let particle_positions = new Float32Array(Module.HEAPF32.buffer, pos_ptr, particle_count * 3);

	const vel_ptr = Module.ccall('get_velocity_buffer', 'number', [], []);
	let particle_velocties = new Float32Array(Module.HEAPF32.buffer, vel_ptr, particle_count * 3);

	const pixels_ptr = Module.ccall("get_led_buffer", 'number', [], []);
	let pixel_buffer = new Float32Array(Module.HEAPF32.buffer, pixels_ptr, 10 * 10 * 6);

	setInterval(() => {
		// initialize_particles();	
	}, 5000);

	const button = gui.add({ action: () => console.log(particle_positions)}, 'action').name('Print Positions');
	const button2 = gui.add({ action: () => console.log(particle_velocties)}, 'action').name('Print Velocities');
	const button3 = gui.add({ action: () => set_control_mode("orbit")}, 'action').name('Orbit Mode');
	const button4 = gui.add({ action: () => set_control_mode("rotate")}, 'action').name('Rotate Mode');
	const button5 = gui.add({ action: () => console.log(pixel_buffer)}, 'action').name('Print Pixels');

	function animate() {
		const down = localDown.clone();
		down.applyQuaternion(cube_group.quaternion.clone().invert()).setLength(settings.gravity);

		tick_particles(1/60, settings.r_force, down.x, down.y, down.z); 

		const positions = line_geometry.attributes.position.array;

		for (let i = 0; i < particle_count; i++)
		{
			particles[i].position.set(
				particle_positions[i*3+0],
				particle_positions[i*3+1],
				particle_positions[i*3+2]
			)	

			positions[i*6+0] = particle_positions[i*3+0];
			positions[i*6+1] = particle_positions[i*3+1];
			positions[i*6+2] = particle_positions[i*3+2];

			let vx =  particle_velocties[i*3+0];
			let vy =  particle_velocties[i*3+1];
			let vz =  particle_velocties[i*3+2];

			let c = Math.sqrt(vx*vx+ vy*vy + vz*vz);


			positions[i*6+3] = particle_positions[i*3+0] + vx / c * 2;
			positions[i*6+4] = particle_positions[i*3+1] + vy / c * 2;
			positions[i*6+5] = particle_positions[i*3+2] + vz / c * 2;
		}

		line_geometry.attributes.position.needsUpdate = true;

		if (control_mode == "orbit")
		{
			orbit.update();
		} else if (control_mode == "rotate") {
		} else if (control_mode == "transform") {
		}


		FrontGrid.set_pixels(pixel_buffer, 0)
		TopGrid.set_pixels(pixel_buffer, 10*10)
		RightGrid.set_pixels(pixel_buffer, 10*10 * 2)
		BottomGrid.set_pixels(pixel_buffer, 10*10 * 3)
		BackGrid.set_pixels(pixel_buffer, 10*10 * 4)
		LeftGrid.set_pixels(pixel_buffer, 10*10 * 5)

		renderer.render( scene, camera );

	}

	renderer.setAnimationLoop( animate );
}

main();

