#include "simulation.h"

#include <stdlib.h>

// Simulation constants
const float particle_size = 1; // Size of particles (radius)
const float box_size = 4; // Distance to each face on the box from the origin point
const float box_padding = box_size - particle_size; // Max distance that a particle should go on any axis from the origin
const float box_size_i = (1 / box_padding); // Inverse of box padding (used to speed up calculations)
const float led_finder_constant = (GRID_SIZE / (box_padding*2)) - 0.0001; // A constant used to find which LED a particle is closest to

const float pixel_multi = 3; // A brightness multiplier

// thanks https://en.wikipedia.org/wiki/Fast_inverse_square_root
float Q_rsqrt( float number )
{
	int i;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;                       // evil floating point bit level hacking
	i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}

#define SIGN(x) (((x) > 0) - ((x) < 0))

float particle_positions[particle_count*3];

float particle_velocities[particle_count*3];

float particle_accelerations[particle_count*3];

float led_grid[RAW_GRID_POINTS];

float *front_grid;
float *top_grid;
float *right_grid;
float *bottom_grid;
float *back_grid;
float *left_grid;

// The offsets of positions the leds 
// X, Y
int led_positions[9*2] = {
	0, 0,
	1, 0,
	0, 1,
	1, 1,
	-1, 0,
	-1, -1,
	1, -1,
	0, -1,
	-1, 1,
};

// The weight of each pixel offset for the led_positions
float led_weights[9] = {0.11, 0.11,0.11,0.11,0.11,0.11,0.11,0.11,0.11};

// Clears the LED Grid
void clear_led_grid()
{
	for (int i = 0; i < RAW_GRID_POINTS; i++)
		led_grid[i] = 0;
}

// Outputs a random number between 0.0-1.0
float random1()
{
	return ((float) rand() / (float) RAND_MAX);
}

void initialize_particles()
{
	// Shortcuts for grid faces in the array
	front_grid = led_grid;
	top_grid = led_grid      + RAW_GRID_SIZE_2;
	right_grid = top_grid    + RAW_GRID_SIZE_2;
	bottom_grid = right_grid + RAW_GRID_SIZE_2;
	back_grid = bottom_grid  + RAW_GRID_SIZE_2;
	left_grid = back_grid    + RAW_GRID_SIZE_2;

	// Seed does not matter or need to be unique
	srand(1234);

	// Setting initial random positions
	for (int i = 0; i < particle_count; i++)
	{
		// Setting Position
		particle_positions[i*3+0] = random1() * box_padding * 2 - box_padding;
		particle_positions[i*3+1] = random1() * box_padding * 2 - box_padding;
		particle_positions[i*3+2] = random1() * box_padding * 2 - box_padding;

		// Setting Velocity
		particle_velocities[i*3+0] = 0;
		particle_velocities[i*3+1] = 0;
		particle_velocities[i*3+2] = 0;
	}
}

// One step of the simulation
void tick_particles(float dt, float r_force, float gx, float gy, float gz)
{
	// Clearing the grid
	clear_led_grid();


	// Random offsets to keep them particles from perfectly stacking
	float random_force = 0.2;
	float rx = random1() * random_force - random_force * 0.5;
	float ry = random1() * random_force - random_force * 0.5;
	float rz = random1() * random_force - random_force * 0.5;

	float dt2_2 = dt*dt * 0.5; // (1/2)t^2
	for (int i = 0 ; i < particle_count; i++)
	{
		// Indexes for the particles
		int ix = i*3;
		int iy = ix+1;
		int iz = iy+1;

		// Looping through every other particle
		for (int j = i+1; j < particle_count; j++)
		{

			// Calculating distances
			float dif_x = particle_positions[j*3+0] - particle_positions[ix] + rx;
			float dif_y = particle_positions[j*3+1] - particle_positions[iy] + ry;
			float dif_z = particle_positions[j*3+2] - particle_positions[iz] + rz;
			float distance_2 = dif_x*dif_x + dif_y*dif_y + dif_z * dif_z;

			// Prevent particles from interacting if they are too far apart
			// Keeps particles from bundling up towards the edges
			if (distance_2 > 2)
				continue;

			// Calculate the force
			float force = -r_force * Q_rsqrt(distance_2 + 0.0001);

			// Adding to the acceleration
			particle_accelerations[ix] += dif_x * force;
			particle_accelerations[iy] += dif_y * force;
			particle_accelerations[iz] += dif_z * force;

			particle_accelerations[j*3+0] -= dif_x * force;
			particle_accelerations[j*3+1] -= dif_y * force;
			particle_accelerations[j*3+2] -= dif_z * force;

		}
	}

	// Looping through all particles again
	for (int i = 0; i < particle_count; i++)
	{

		int ix = i*3;
		int iy = ix+1;
		int iz = iy+1;

		// Updating particle positions and velocites
		particle_positions[ix] += particle_accelerations[ix] * dt2_2 + particle_velocities[ix] * dt;
		particle_positions[iy] += particle_accelerations[iy] * dt2_2 + particle_velocities[iy] * dt;
		particle_positions[iz] += particle_accelerations[iz] * dt2_2 + particle_velocities[iz] * dt;

		particle_velocities[ix] = particle_velocities[ix] * PARTICLE_DAMPENING + particle_accelerations[ix] * dt;
		particle_velocities[iy] = particle_velocities[iy] * PARTICLE_DAMPENING + particle_accelerations[iy] * dt;
		particle_velocities[iz] = particle_velocities[iz] * PARTICLE_DAMPENING + particle_accelerations[iz] * dt;


		

		// Bounds Check
		if (particle_positions[ix]  > box_padding)
		{
			particle_positions[ix] = box_padding;
			particle_velocities[ix] = 0;
		}
		if (particle_positions[ix]  < -box_padding)
		{
			particle_positions[ix] = -box_padding;
			particle_velocities[ix] = 0;
		}

		if (particle_positions[iy]  > box_padding)
		{
			particle_positions[iy] = box_padding;
			particle_velocities[iy] = 0;
		}

		if (particle_positions[iy]  < -box_padding)
		{
			particle_positions[iy] = -box_padding;
			particle_velocities[iy] = 0;
		}

		if (particle_positions[iz]  > box_padding)
		{
			particle_positions[iz] = box_padding;
			particle_velocities[iz] = 0;
		}
		if (particle_positions[iz]  < -box_padding)
		{
			particle_positions[iz] = -box_padding;
			particle_velocities[iz] = 0;
		}


		// Reseting particle acceleration
		particle_accelerations[ix] = gx;
		particle_accelerations[iy] = gy;
		particle_accelerations[iz] = gz;


		// Filling LED Grid
		
		// getting position on each axis
		int led_x = (int) ((particle_positions[ix] + box_padding) * led_finder_constant) + 1;
		int led_y = (int) ((particle_positions[iy] + box_padding) * led_finder_constant) + 1;
		int led_z = (int) ((particle_positions[iz] + box_padding) * led_finder_constant) + 1;

		// Weight on each axis
		float x_value = (box_padding+particle_positions[ix]) * (0.5 * box_size_i);
		float y_value = (box_padding+particle_positions[iy]) * (0.5 * box_size_i);
		float z_value = (box_padding+particle_positions[iz]) * (0.5 * box_size_i);

		// Looping through the 3x3 Grid
		for (int j = 0; j < 9; j++)
		{
			// offset from grid
			// Because it is symetric on all axes we do not have to account for any weird stuff
			int o = led_positions[j*2] + led_positions[j*2+1] * RAW_GRID_SIZE;
			float weight = led_weights[j] * pixel_multi;


			// Setting the brightness of the leds
			front_grid[led_x + led_y * RAW_GRID_SIZE + o]                        += z_value * weight; // Front
			top_grid[led_x + (RAW_GRID_SIZE - led_z-1) * RAW_GRID_SIZE + o]      += y_value * weight; // Top
			right_grid[(RAW_GRID_SIZE - led_z - 1) + led_y * RAW_GRID_SIZE + o]  += x_value * weight; // Right
			bottom_grid[led_x + led_z * RAW_GRID_SIZE + o]                       += (1 - y_value) * weight; // Bottom
			back_grid[(RAW_GRID_SIZE - led_x) + led_y * RAW_GRID_SIZE + o]       += (1 - z_value) * weight; // Back
			left_grid[led_z + led_y * RAW_GRID_SIZE + o]                         += (1 - x_value) * weight; // Left

		}
	}
}
