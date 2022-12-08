// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
#ifndef __D3DPARTICLE_H__
#define __D3DPARTICLE_H__

#define SANDSTORM_R		226
#define SANDSTORM_G		153
#define SANDSTORM_B		6
#define SANDSTORM_A		255

#define RAIN_R			175
#define RAIN_G			228
#define RAIN_B			249
#define RAIN_A			100

#define SNOW_R			255
#define SNOW_G			255
#define SNOW_B			255
#define SNOW_A			220

#define FIREWORKS_R			((int)rand() % 256)
#define FIREWORKS_G			((int)rand() % 256)
#define FIREWORKS_B			((int)rand() % 256)
#define FIREWORKS_A			220

typedef struct particle
{
	int			energy;
	custom_xyz	pos;
	custom_xyz	oldPos;
	custom_xyz	velocity;
	custom_xyz	rotation;
	custom_bgra	bgra;
} particle;

typedef struct emitter
{
	int			numParticles;
   int         numAlive;
   int         maxParticles;
	int			energy;
	int			timer;
	int			timerBase;
	int			randomPos;
	int			randomRot;
   int         emitterFlags;
	custom_xyz	pos;
	custom_xyz	delta;
	custom_xyz	velocity;
	custom_xyz	rotation;
	custom_bgra	bgra;
	particle	*particles;
} emitter;

#define PS_NO_FLAGS        0
#define PS_RANDOM_XY       0x0001
#define PS_RANDOM_Z        0x0002
#define PS_RANDOM_CIRCLE   0x0004
#define PS_CIRCLE          0x0008
#define PS_CIRCLE_X        0x0010
#define PS_CIRCLE_Y        0x0020
#define PS_CIRCLE_Z        0x0040
#define PS_GROUND_DESTROY  0x0080
#define PS_WEATHER_EFFECT  0x0100
#define PS_GRAVITY         0x0200

typedef struct particle_system
{
   // Rendering info.
   D3DPRIMITIVETYPE drawType;
   int numDrawStages;
   u_int numIndices;
   u_int numVertices;
   u_int numPrimitives;
   custom_xyz pParticleXYZ[4];
   custom_st pParticleST[4];
   custom_index pParticleIndices[4];
   LPDIRECT3DTEXTURE9 pTexture;

   // Particle info.
   int numParticles;
   list_type emitterList;
} particle_system;

// Particle system init/reset.
void D3DParticleSystemInit(void); // System static info.
void D3DParticleSystemShutdown(void); // System static info.
void D3DParticlesInit(bool reset); // Emitters.

// Particle system update functions.
void D3DParticleSystemSetPlayerPos(float posX, float posY, float posZ);
void	D3DParticleSystemUpdateFluid(particle_system *pParticleSystem, d3d_render_pool_new *pPool,
							 d3d_render_cache_system *pCacheSystem, Draw3DParams *params);
void	D3DParticleSystemUpdateBurst(particle_system *pParticleSystem, d3d_render_pool_new *pPool,
							 d3d_render_cache_system *pCacheSystem, Draw3DParams *params);

#endif
