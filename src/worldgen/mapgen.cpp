/*
Copyright (c) 2010, The Mineserver Project
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of the The Mineserver Project nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <cmath>
#include <ctime>


#include "../logger.h"
#include "../constants.h"

#include "../config.h"
#include "../nbt.h"
#include "../map.h"
#include "../tree.h"

// libnoise
#ifdef DEBIAN
#include <libnoise/noise.h>
#else
#include <noise/noise.h>
#endif

#include "../logger.h"
#include "../constants.h"
#include "../config.h"
#include "../nbt.h"
#include "../map.h"
#include "../mineserver.h"

//#include "mersenne.h"
#include "cavegen.h"
#include "mapgen.h"

void MapGen::init(int seed)
{
	cave.init(seed+8);

	ridgedMultiNoise.SetSeed(seed);
	ridgedMultiNoise.SetOctaveCount(6);
	ridgedMultiNoise.SetFrequency(1.0/180.0);
	ridgedMultiNoise.SetLacunarity(2.0);
	//  perlinNoise.SetPersistence(0.1); // 0-1

	// Heighmap scale..
	perlinScale = 0.7f;

	baseFlatTerrain.SetSeed(seed+1);
	baseFlatTerrain.SetOctaveCount(2);
	baseFlatTerrain.SetFrequency(0.5);
	baseFlatTerrain.SetPersistence(0.2);

	flatTerrain.SetSourceModule(0, baseFlatTerrain);
	//flatTerrain.SetScale(0.125);
	flatTerrain.SetBias(0.4);

	seaFloor.SetSeed(seed+3);
	seaFloor.SetOctaveCount(1);
	seaFloor.SetPersistence(0.15);

	seaBias.SetSourceModule(0, seaFloor);
	seaBias.SetBias(-1.00);

	terrainType.SetSeed(seed+2);
	terrainType.SetFrequency(0.5);
	terrainType.SetPersistence(0.10);

	seaControl.SetSeed(seed+4);
	seaControl.SetFrequency(0.15);

	finalTerrain.SetSourceModule(0, flatTerrain);
	finalTerrain.SetSourceModule(1, perlinBiased);
	finalTerrain.SetControlModule(terrainType);
	finalTerrain.SetBounds(0.25, 1000.0);
	finalTerrain.SetEdgeFalloff(0.125);

	seaTerrain.SetSourceModule(0, seaBias);
	seaTerrain.SetSourceModule(1, finalTerrain);
	seaTerrain.SetControlModule(seaControl);
	seaTerrain.SetBounds(-0.3, 1000.0);
	seaTerrain.SetEdgeFalloff(0.1);

	seaLevel = Mineserver::get()->conf()->iValue("sea_level");

	m_seed = seed;
	treesGenerated = false;
}

void MapGen::generateFlatgrass() 
{
	for (int bX = 0; bX < 16; bX++) 
	{
		for (int bY = 0; bY < 128; bY++) 
		{
			for (int bZ = 0; bZ < 16; bZ++) 
			{
				if (bY == 0) 
					blocks[bY + (bZ * 128 + (bX * 128 * 16))] = BLOCK_BEDROCK; 
				else if (bY < 64) 
					blocks[bY + (bZ * 128 + (bX * 128 * 16))] = BLOCK_DIRT;
				else if (bY == 64) 
					blocks[bY + (bZ * 128 + (bX * 128 * 16))] = BLOCK_GRASS;
				else 
					blocks[bY + (bZ * 128 + (bX * 128 * 16))] = BLOCK_AIR;
			}
		}
	}
}

void MapGen::generateChunk(int x, int z)
{
	NBT_Value *main = new NBT_Value(NBT_Value::TAG_COMPOUND);
	NBT_Value *val = new NBT_Value(NBT_Value::TAG_COMPOUND);

	if(Mineserver::get()->conf()->bValue("map_flatgrass"))
		generateFlatgrass();
	else
		generateWithNoise(x, z);

	val->Insert("Blocks", new NBT_Value(blocks, 16*16*128));
	val->Insert("Data", new NBT_Value(blockdata, 16*16*128/2));
	val->Insert("SkyLight", new NBT_Value(skylight, 16*16*128/2));
	val->Insert("BlockLight", new NBT_Value(blocklight, 16*16*128/2));
	val->Insert("HeightMap", new NBT_Value(heightmap, 16*16));
	val->Insert("Entities", new NBT_Value(NBT_Value::TAG_LIST, NBT_Value::TAG_COMPOUND));
	val->Insert("TileEntities", new NBT_Value(NBT_Value::TAG_LIST, NBT_Value::TAG_COMPOUND));
	val->Insert("LastUpdate", new NBT_Value((sint64)time(NULL)));
	val->Insert("xPos", new NBT_Value(x));
	val->Insert("zPos", new NBT_Value(z));
	val->Insert("TerrainPopulated", new NBT_Value((char)1));

	main->Insert("Level", val);

	/*  uint32 chunkid;
	Mineserver::get()->map()->posToId(x, z, &chunkid);

	Mineserver::get()->map()->maps[chunkid].x = x;
	Mineserver::get()->map()->maps[chunkid].z = z; */

	std::vector<uint8> *t_blocks = (*val)["Blocks"]->GetByteArray();
	std::vector<uint8> *t_data = (*val)["Data"]->GetByteArray();
	std::vector<uint8> *t_blocklight = (*val)["BlockLight"]->GetByteArray();
	std::vector<uint8> *t_skylight = (*val)["SkyLight"]->GetByteArray();
	std::vector<uint8> *heightmap = (*val)["HeightMap"]->GetByteArray();

	sChunk *chunk = new sChunk();
	chunk->blocks = &((*t_blocks)[0]);
	chunk->data = &((*t_data)[0]);
	chunk->blocklight = &((*t_blocklight)[0]);
	chunk->skylight = &((*t_skylight)[0]);
	chunk->heightmap = &((*heightmap)[0]);
	chunk->nbt = main;
	chunk->x = x;
	chunk->z = z;

	Mineserver::get()->map()->chunks.LinkChunk(chunk, x, z);

	// Update last used time
	//Mineserver::get()->map()->mapLastused[chunkid] = (int)time(0);

	// Not changed
	//Mineserver::get()->map()->mapChanged[chunkid] = Mineserver::get()->conf()->bValue("save_unchanged_chunks");

	//Mineserver::get()->map()->maps[chunkid].nbt = main;
}

//#define PRINT_MAPGEN_TIME


void MapGen::AddTrees() {
	if(!treesGenerated) {
		int x = 0;
		int y = 65;
		int z = 0;
		for(int treeZ = 0; treeZ < 100; treeZ+=5) {
			for(int treeLoc = 0; treeLoc < 100; treeLoc+=5) {
				Tree tree(treeLoc,y,treeZ);
				tree.generate();
			}
		}
	}
	treesGenerated = true;
}

void MapGen::generateWithNoise(int x, int z) 
{
	// Debug..
#ifdef PRINT_MAPGEN_TIME
#ifdef WIN32    
	DWORD t_begin,t_end;
	t_begin = timeGetTime ();
#else
	struct timeval start, end;
	gettimeofday(&start, NULL);
#endif
#endif

	// Populate blocks in chunk
	sint32 currentHeight;
	sint32 ymax;
	uint8 *curBlock;
	memset(blocks, 0, 16*16*128);

	double xBlockpos=x<<4;
	double zBlockpos=z<<4;
	for (int bX = 0; bX < 16; bX++) 
	{
		for (int bZ = 0; bZ < 16; bZ++) 
		{

			heightmap[(bZ<<4)+bX] = ymax = currentHeight = (uint8)((ridgedMultiNoise.GetValue(xBlockpos+bX,0, zBlockpos+bZ) * 15) + 64);

			sint32 stoneHeight = (sint32)(currentHeight * 0.94);
			sint32 bYbX = ((bZ << 7) + (bX << 11));

			if(ymax < seaLevel) 
				ymax = seaLevel;

			for(int bY = 0; bY <= ymax; bY++) 
			{
				curBlock = &blocks[bYbX++];

				// Place bedrock
				if(bY == 0) 
				{
					*curBlock = BLOCK_BEDROCK;
					continue;
				}

				if(bY < currentHeight) 
				{
					if (bY < stoneHeight)
					{
						*curBlock = BLOCK_STONE;
						// Add caves
						cave.AddCaves(*curBlock, xBlockpos + bX, bY, zBlockpos + bZ);
					}
					else
						*curBlock = BLOCK_DIRT;
				} 
				else if(currentHeight == bY)
				{
					if (bY == seaLevel || bY == seaLevel - 1 || bY == seaLevel - 2)
						*curBlock = BLOCK_SAND; // FF
					else if (bY < seaLevel - 1)
						*curBlock = BLOCK_GRAVEL; // FF
					else
						*curBlock = BLOCK_GRASS; // FF
				} 
				else 
				{
					if (bY <= seaLevel)
						*curBlock = BLOCK_STATIONARY_WATER; // FF
					else
						*curBlock = BLOCK_AIR; // FF
				}        
			}
		}
	}
	if(Mineserver::get()->conf()->bValue("add_beaches"))
		AddBeaches();

#ifdef PRINT_MAPGEN_TIME
#ifdef WIN32
	t_end = timeGetTime ();
	Mineserver::get()->screen()->log("Mapgen: " + dtos(t_end-t_begin) + "ms");
#else
	gettimeofday(&end, NULL);
	Mineserver::get()->screen()->log("Mapgen: " + dtos(end.tv_usec - start.tv_usec));
#endif
#endif
}

void MapGen::AddBeaches() 
{
	int beachExtent = Mineserver::get()->conf()->iValue("beach_extent");
	int beachHeight = Mineserver::get()->conf()->iValue("beach_height");

	int beachExtentSqr = (beachExtent + 1) * (beachExtent + 1);
	for(int x = 0; x < 16; x++) 
	{
		for(int z = 0; z < 16; z++) 
		{
			int h = -1;
			h = heightmap[(z<<4)+x];
			if(h < 0) continue;

			bool found = false;
			for(int dx = -beachExtent; !found && dx <= beachExtent; dx++) 
			{
				for(int dz = -beachExtent; !found && dz <= beachExtent; dz++) 
				{
					for(int dh = -beachHeight; !found && dh <= 0; dh++) 
					{
						if(dx * dx + dz * dz + dh * dh > beachExtentSqr) continue;
						int xx = x + dx;
						int zz = z + dz;
						int hh = h + dh;
						if(xx < 0 || xx >= 15 || zz < 0 || zz >= 15 || hh < 0 || hh >= 127 ) continue;
						int index = hh + (zz * 128 + (xx * 128 * 16));
						if( blocks[index] == BLOCK_WATER || blocks[index] == BLOCK_STATIONARY_WATER ) {
							found = true;
							break;
						}
					}
				}
			}
			if( found ) {
				uint8 block;
				uint8 meta;

				Mineserver::get()->map()->sendBlockChange(x, h, z, BLOCK_WATER, 0);
				Mineserver::get()->map()->setBlock(x, h, z, BLOCK_WATER, 0);

				Mineserver::get()->map()->getBlock(x, h-1, z, &block, &meta);

				if( h > 0 && block == BLOCK_DIRT )
				{
					Mineserver::get()->map()->sendBlockChange(x, h-1, z, BLOCK_WATER, 0);
					Mineserver::get()->map()->setBlock(x, h-1, z, BLOCK_WATER, 0);
				}
			}
		}
	}
}
