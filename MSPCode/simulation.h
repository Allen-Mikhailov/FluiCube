#ifndef SIMULATION_H
#define SIMULATION_H

#define GRID_SIZE 8
#define GRID_SIZE_2 (GRID_SIZE * GRID_SIZE)
#define GRID_POINTS (GRID_SIZE*GRID_SIZE*6)

#define RAW_GRID_SIZE (GRID_SIZE+2)
#define RAW_GRID_SIZE_2 (RAW_GRID_SIZE*RAW_GRID_SIZE)
#define RAW_GRID_POINTS (RAW_GRID_SIZE_2*6)

#define particle_count 30 
#define PARTICLE_DAMPENING 0.95

extern float r_led_grid[RAW_GRID_POINTS];
extern float g_led_grid[RAW_GRID_POINTS];
extern float b_led_grid[RAW_GRID_POINTS];

void tick_particles(float dt, float r_force, float gx, float gy, float gz);
void initialize_particles();

void clear_led_grid();

#endif
