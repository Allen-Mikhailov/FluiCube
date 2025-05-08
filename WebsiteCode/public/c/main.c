#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <emscripten.h>

#define GRID_SIZE 8
#define GRID_SIZE_2 (GRID_SIZE * GRID_SIZE)
#define GRID_POINTS (GRID_SIZE*GRID_SIZE*6)

#define RAW_GRID_SIZE (GRID_SIZE+2)
#define RAW_GRID_SIZE_2 (RAW_GRID_SIZE*RAW_GRID_SIZE)
#define RAW_GRID_POINTS (RAW_GRID_SIZE_2*6)

#define particle_count 20 
#define PARTICLE_DAMPENING 0.95
const float particle_size = 1;
const float push_force = 1;
const float box_size = 4;
const float box_padding = box_size - particle_size;
const float box_size_i = (1 / box_padding);
const float led_finder_constant = (GRID_SIZE / (box_padding*2)) - 0.0001;

const float box_force = 4;

const float pixel_multi = 3;

// thanks https://en.wikipedia.org/wiki/Fast_inverse_square_root
float Q_rsqrt( float number )
{
	int32_t i;
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

EMSCRIPTEN_KEEPALIVE
float particle_positions[particle_count*3];

EMSCRIPTEN_KEEPALIVE
float particle_velocities[particle_count*3];

EMSCRIPTEN_KEEPALIVE
float particle_accelerations[particle_count*3];


EMSCRIPTEN_KEEPALIVE
float led_grid[RAW_GRID_POINTS];

float *front_grid;
float *top_grid;
float *right_grid;
float *bottom_grid;
float *back_grid;
float *left_grid;

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

float led_weights[9] = {0.11, 0.11,0.11,0.11,0.11,0.11,0.11,0.11,0.11};

void clear_led_grid()
{
	for (int i = 0; i < RAW_GRID_POINTS; i++)
		led_grid[i] = 0;
}

float random1()
{
	return ((float) rand() / (float) RAND_MAX);
}

EMSCRIPTEN_KEEPALIVE
float * get_position_buffer()
{
	return particle_positions;
}

EMSCRIPTEN_KEEPALIVE
float * get_velocity_buffer()
{
	return particle_velocities;
}

EMSCRIPTEN_KEEPALIVE
float * get_acceleration_buffer()
{
	return particle_accelerations;
}

EMSCRIPTEN_KEEPALIVE
float *get_led_buffer()
{
	return led_grid;
}

EMSCRIPTEN_KEEPALIVE
void initialize_particles()
{
	for (int i = 0; i < particle_count; i++)
	{
		particle_positions[i*3+0] = random1() * box_padding * 2 - box_padding;
		particle_positions[i*3+1] = random1() * box_padding * 2 - box_padding;
		particle_positions[i*3+2] = random1() * box_padding * 2 - box_padding;
		particle_velocities[i*3+0] = 0;
		particle_velocities[i*3+1] = 0;
		particle_velocities[i*3+2] = 0;
	}
}


EMSCRIPTEN_KEEPALIVE
void tick_particles(float dt, float r_force, float gx, float gy, float gz)
{
	clear_led_grid();

	float random_force = 0.2;
	float rx = random1() * random_force - random_force / 2;
	float ry = random1() * random_force - random_force / 2;
	float rz = random1() * random_force - random_force / 2;

	float dt2_2 = dt*dt * 0.5;
	for (int i = 0 ; i < particle_count; i++)
	{
		int ix = i*3;
		int iy = ix+1;
		int iz = iy+1;

		


		for (int j = i+1; j < particle_count; j++)
		{

			float dif_x = particle_positions[j*3+0] - particle_positions[ix] + rx;
			float dif_y = particle_positions[j*3+1] - particle_positions[iy] + ry;
			float dif_z = particle_positions[j*3+2] - particle_positions[iz] + rz;
			float distance_2 = dif_x*dif_x + dif_y*dif_y + dif_z * dif_z;

			if (distance_2 > 2)
				continue;

			float force = -r_force * Q_rsqrt(distance_2 + 0.0001);

			particle_accelerations[ix] += dif_x * force;
			particle_accelerations[iy] += dif_y * force;
			particle_accelerations[iz] += dif_z * force;

			particle_accelerations[j*3+0] -= dif_x * force;
			particle_accelerations[j*3+1] -= dif_y * force;
			particle_accelerations[j*3+2] -= dif_z * force;

		}
	}

	for (int i = 0; i < particle_count; i++)
	{

		int ix = i*3;
		int iy = ix+1;
		int iz = iy+1;

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


		particle_accelerations[ix] = gx;
		particle_accelerations[iy] = gy;
		particle_accelerations[iz] = gz;


		// Filling LED Grid
		int led_x = (int) ((particle_positions[ix] + box_padding) * led_finder_constant) + 1;
		int led_y = (int) ((particle_positions[iy] + box_padding) * led_finder_constant) + 1;
		int led_z = (int) ((particle_positions[iz] + box_padding) * led_finder_constant) + 1;

		float x_value = (box_padding+particle_positions[ix]) * (0.5 * box_size_i);
		float y_value = (box_padding+particle_positions[iy]) * (0.5 * box_size_i);
		float z_value = (box_padding+particle_positions[iz]) * (0.5 * box_size_i);

		for (int j = 0; j < 9; j++)
		{
			int o = led_positions[j*2] + led_positions[j*2+1] * RAW_GRID_SIZE;
			float weight = led_weights[j] * pixel_multi;


			front_grid[led_x + led_y * RAW_GRID_SIZE + o]                    += z_value * weight; // Front
			top_grid[led_x + (RAW_GRID_SIZE - led_z-1) * RAW_GRID_SIZE + o]                      += y_value * weight; // Top
			right_grid[(RAW_GRID_SIZE - led_z - 1) + led_y * RAW_GRID_SIZE + o]  += x_value * weight; // Right
			bottom_grid[led_x + led_z * RAW_GRID_SIZE + o] += (1 - y_value) * weight; // Bottom
			back_grid[(RAW_GRID_SIZE - led_x) + led_y * RAW_GRID_SIZE + o]   += (1 - z_value) * weight; // Back
			left_grid[led_z + led_y * RAW_GRID_SIZE + o]                     += (1 - x_value) * weight; // Left

		}
	}

			
}

int main() {
	front_grid = led_grid;
	top_grid = led_grid      + RAW_GRID_SIZE_2;
	right_grid = top_grid    + RAW_GRID_SIZE_2;
	bottom_grid = right_grid + RAW_GRID_SIZE_2;
	back_grid = bottom_grid  + RAW_GRID_SIZE_2;
	left_grid = back_grid    + RAW_GRID_SIZE_2;

	// front_grid_2 = led_grid_2;
	// top_grid_2 = led_grid_2 + GRID_2_SIZE_2;
	// right_grid_2 = top_grid_2 + GRID_2_SIZE_2;
	// bottom_grid_2 = right_grid_2 + GRID_2_SIZE_2;
	// back_grid_2 = bottom_grid_2 + GRID_2_SIZE_2;
	// left_grid_2 = back_grid_2 + GRID_2_SIZE_2;



	srand(time(NULL));
}

