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

#ifdef DEBIAN
#include <libnoise/noise.h>
#else
#include <noise/noise.h>
#endif

#include "../logger.h"
#include "../constants.h"
#include "../tools.h"
#include "../map.h"
#include "../config.h"
#include "../mineserver.h"

#include "mersenne.h"

#include "cavegen.h"

void CaveGen::init(int seed)
{
  // Set up us the Perlin-noise module.
  caveNoise1.SetSeed (seed+1);
  caveNoise1.SetFrequency (1.0/20);
  //caveNoise.SetLacunarity (0.5);
  caveNoise1.SetOctaveCount (2);

  caveNoise1.SetNoiseQuality (noise::QUALITY_STD);
  
  // Set up us the Perlin-noise module.
  caveNoise2.SetSeed (seed+2);
  caveNoise2.SetFrequency (1.0/20);
  //caveNoise.SetLacunarity (0.5);
  caveNoise2.SetOctaveCount (2);

  caveNoise2.SetNoiseQuality (noise::QUALITY_STD);

  addCaves = Mineserver::get()->conf()->bValue("add_caves");
  caveDensity = Mineserver::get()->conf()->iValue("cave_density");
  caveSize = Mineserver::get()->conf()->iValue("cave_size");
  addCaveLava = Mineserver::get()->conf()->bValue("cave_lava");
  addCaveWater = Mineserver::get()->conf()->bValue("cave_water");
  addOre = Mineserver::get()->conf()->bValue("cave_ore");

  seaLevel = Mineserver::get()->conf()->iValue("sea_level");
}

void CaveGen::AddCaves(uint8 &block, double x, double y, double z)
{ 
  if(addCaves)
  {
   
    caveN1 = caveNoise1.GetValue(x,y,z);
    
    if(caveN1 < 0.1)// && block != BLOCK_WATER && block != BLOCK_STATIONARY_WATER)
    {
      
      // Add bottomlava
      if(y < 10.0 && addCaveLava)
      {
        block = BLOCK_STATIONARY_LAVA;
        return;
      }
      
      block = BLOCK_AIR;
      return;
    }

    
    /*if(y < 60.0 && addOre)
    {      
      caveN1 = caveNoise1.GetValue(x,y,z);
      if(caveN1 > 0.56)
      {
        if(y < 32.0 && caveN1 > 0.67)
        {
          if(y < 16.0 && caveN1 > 0.79)
          {
            block = BLOCK_DIAMOND_ORE;
            return;
          }
          block = BLOCK_GOLD_ORE;
          return;
        }        
        block = BLOCK_IRON_ORE;
      }
      return;
    }*/

  }
}
