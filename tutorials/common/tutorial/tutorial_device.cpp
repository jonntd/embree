// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "tutorial_device.h"
#include "../scenegraph/texture.h"
#include "scene_device.h"

#if defined(__cplusplus)
namespace embree {
#endif

/* the scene to render */
extern RTCScene g_scene;

/* global subdivision level for subdivision geometry */
unsigned int g_subdivision_levels = 0;

/* intensity scaling for traversal cost visualization */
//float scale = 0.001f;
float scale = 1.0f / 1000000.0f;

extern "C" {
  bool g_changed = false;
}
extern "C" float g_debug;

/* error reporting function */
void error_handler(const RTCError code, const char* str)
{
  if (code == RTC_NO_ERROR) 
    return;

  printf("Embree: ");
  switch (code) {
  case RTC_UNKNOWN_ERROR    : printf("RTC_UNKNOWN_ERROR"); break;
  case RTC_INVALID_ARGUMENT : printf("RTC_INVALID_ARGUMENT"); break;
  case RTC_INVALID_OPERATION: printf("RTC_INVALID_OPERATION"); break;
  case RTC_OUT_OF_MEMORY    : printf("RTC_OUT_OF_MEMORY"); break;
  case RTC_UNSUPPORTED_CPU  : printf("RTC_UNSUPPORTED_CPU"); break;
  case RTC_CANCELLED        : printf("RTC_CANCELLED"); break;
  default                   : printf("invalid error code"); break;
  }
  if (str) { 
    printf(" ("); 
    while (*str) putchar(*str++); 
    printf(")\n"); 
  }
  exit(1);
}

/* stores pointer to currently used rendeTile function */
renderTileFunc renderTile;

/* renders a single pixel with eyelight shading */
Vec3fa renderPixelEyeLight(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(0.0f);
  else return Vec3fa(embree::abs(dot(ray.dir,normalize(ray.Ng))));
}

void renderTileEyeLight(int taskIndex, int* pixels, 
                        const unsigned int width, const unsigned int height, 
                        const float time, const ISPCCamera& camera,
                        const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelEyeLight(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

__noinline void setray(RTCRay& ray)
{
  ray.u = ray.v = 0.001f;
  ray.Ng = Vec3fa(0,1,0);
  ray.geomID = 0;
  ray.primID = 0;
}

/* renders a single pixel with wireframe shading */
Vec3fa renderPixelWireframe(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);

  /* return black if nothing hit */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(1.0f);

  /* calculate wireframe around triangles */
  const float border = 0.05f;
  Vec3fa color = Vec3fa(1.0f);
  if (ray.u < border) color = Vec3fa(0.0f);
  if (ray.v < border) color = Vec3fa(0.0f);
  if (1.0f-ray.u-ray.v < border) color = Vec3fa(0.0f);

  /* perform eyelight shading */
  return color*Vec3fa(embree::abs(dot(ray.dir,normalize(ray.Ng))));
}

void renderTileWireframe(int taskIndex, int* pixels, 
                         const unsigned int width, const unsigned int height, 
                         const float time, const ISPCCamera& camera,
                         const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelWireframe(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

/* renders a single pixel with UV shading */
Vec3fa renderPixelUV(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(0.0f);
  else return Vec3fa(ray.u,ray.v,1.0f-ray.u-ray.v);
}

void renderTileUV(int taskIndex, int* pixels, 
                  const unsigned int width, const unsigned int height, 
                  const float time, const ISPCCamera& camera,
                  const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelUV(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

/* renders a single pixel with geometry normal shading */
Vec3fa renderPixelNg(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(0.0f);
  else {
    //if (dot(ray.dir,ray.Ng) > 0.0f) return Vec3fa(zero); else
    return normalize(abs(Vec3fa(ray.Ng.x,ray.Ng.y,ray.Ng.z)));
  }
}

void renderTileNg(int taskIndex, int* pixels, 
                  const unsigned int width, const unsigned int height, 
                  const float time, const ISPCCamera& camera,
                  const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelNg(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

Vec3fa randomColor(const int ID) 
{
  int r = ((ID+13)*17*23) >> 8 & 255;
  int g = ((ID+15)*11*13) >> 8 & 255;
  int b = ((ID+17)* 7*19) >> 8 & 255;
  const float oneOver255f = 1.f/255.f;
  return Vec3fa(r*oneOver255f,g*oneOver255f,b*oneOver255f);
}

/* geometry ID shading */
Vec3fa renderPixelGeomID(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(0.0f);
  else return embree::abs(dot(ray.dir,normalize(ray.Ng)))*randomColor(ray.geomID);
}

void renderTileGeomID(int taskIndex, int* pixels, 
                      const unsigned int width, const unsigned int height, 
                      const float time, const ISPCCamera& camera,
                      const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelGeomID(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

/* geometry ID and primitive ID shading */
Vec3fa renderPixelGeomIDPrimID(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(0.0f);
  else return randomColor(ray.geomID ^ ray.primID)*Vec3fa(embree::abs(dot(ray.dir,normalize(ray.Ng))));
}

void renderTileGeomIDPrimID(int taskIndex, int* pixels, 
                            const unsigned int width, const unsigned int height, 
                            const float time, const ISPCCamera& camera,
                            const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelGeomIDPrimID(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

/* vizualizes the traversal cost of a pixel */
Vec3fa renderPixelCycles(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  int64_t c0 = get_tsc();
  rtcIntersect(g_scene,ray);
  int64_t c1 = get_tsc();
  /* shade pixel */
  return Vec3fa((float)(c1-c0)*scale,0.0f,0.0f);
}

void renderTileCycles(int taskIndex, int* pixels, 
                      const unsigned int width, const unsigned int height, 
                      const float time, const ISPCCamera& camera,
                      const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelCycles(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

/* renders a single pixel with UV shading */
Vec3fa renderPixelUV16(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  for (int i=0; i<16; i++) {
    ray.tfar = inf;
    rtcIntersect(g_scene,ray);
  }

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(0.0f);
  else return Vec3fa(ray.u,ray.v,1.0f-ray.u-ray.v);
}

void renderTileUV16(int taskIndex, int* pixels, 
                    const unsigned int width, const unsigned int height, 
                      const float time, const ISPCCamera& camera,
                      const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelUV16(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

/* renders a single pixel casting with ambient occlusion */
Vec3fa renderPixelAmbientOcclusion(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(0.0f);

  Vec3fa Ng = normalize(ray.Ng);
  Vec3fa col = Vec3fa(min(1.f,.3f+.8f*abs(dot(Ng,normalize(ray.dir)))));

  /* calculate hit point */
  float intensity = 0;
  Vec3fa hitPos = ray.org + ray.tfar * ray.dir;

#define AMBIENT_OCCLUSION_SAMPLES 64

  /* trace some ambient occlusion rays */
  int seed = 34*x+12*y;
  for (int i=0; i<AMBIENT_OCCLUSION_SAMPLES; i++) 
  {
    Vec3fa dir; 
    const float oneOver10000f = 1.f/10000.f;
    seed = 1103515245 * seed + 12345;
    dir.x = (seed%10000)*oneOver10000f;
    seed = 1103515245 * seed + 12345;
    dir.y = (seed%10000)*oneOver10000f;
    seed = 1103515245 * seed + 12345;
    dir.z = (seed%10000)*oneOver10000f;
    
    /* initialize shadow ray */
    RTCRay shadow;
    shadow.org = hitPos;
    shadow.dir = dir;
    shadow.tnear = 0.001f;
    shadow.tfar = inf;
    shadow.geomID = RTC_INVALID_GEOMETRY_ID;
    shadow.primID = RTC_INVALID_GEOMETRY_ID;
    shadow.mask = -1;
    shadow.time = 0;
    
    /* trace shadow ray */
    rtcOccluded(g_scene,shadow);
    
    /* add light contribution */
    if (shadow.geomID == RTC_INVALID_GEOMETRY_ID)
      intensity += 1.0f;   
  }

  intensity *= 1.0f/AMBIENT_OCCLUSION_SAMPLES;

  /* shade pixel */
  return col * intensity;
}

void renderTileAmbientOcclusion(int taskIndex, int* pixels, 
                                const unsigned int width, const unsigned int height, 
                                const float time, const ISPCCamera& camera,
                                const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelAmbientOcclusion(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

/* differential visualization */
static int differentialMode = 0;
Vec3fa renderPixelDifferentials(float x, float y, const ISPCCamera& camera)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) return Vec3fa(0.0f);
  
  /* calculate differentials */
  float eps = 0.001f/16.0f;
  Vec3fa P00, P01, P10, P11;
  Vec3fa dP00du, dP01du, dP10du, dP11du;
  Vec3fa dP00dv, dP01dv, dP10dv, dP11dv;
  Vec3fa dPdu1, dPdv1, ddPdudu1, ddPdvdv1, ddPdudv1;
  rtcInterpolate(g_scene,ray.geomID,ray.primID,ray.u+0.f,ray.v+0.f,RTC_VERTEX_BUFFER0,&P00.x,&dP00du.x,&dP00dv.x,3);
  rtcInterpolate(g_scene,ray.geomID,ray.primID,ray.u+0.f,ray.v+eps,RTC_VERTEX_BUFFER0,&P01.x,&dP01du.x,&dP01dv.x,3);
  rtcInterpolate(g_scene,ray.geomID,ray.primID,ray.u+eps,ray.v+0.f,RTC_VERTEX_BUFFER0,&P10.x,&dP10du.x,&dP10dv.x,3);
  rtcInterpolate(g_scene,ray.geomID,ray.primID,ray.u+eps,ray.v+eps,RTC_VERTEX_BUFFER0,&P11.x,&dP11du.x,&dP11dv.x,3);
  rtcInterpolate2(g_scene,ray.geomID,ray.primID,ray.u,ray.v,RTC_VERTEX_BUFFER0,nullptr,&dPdu1.x,&dPdv1.x,&ddPdudu1.x,&ddPdvdv1.x,&ddPdudv1.x,3);
  Vec3fa dPdu0 = (P10-P00)/eps;
  Vec3fa dPdv0 = (P01-P00)/eps;
  Vec3fa ddPdudu0 = (dP10du-dP00du)/eps;
  Vec3fa ddPdvdv0 = (dP01dv-dP00dv)/eps;
  Vec3fa ddPdudv0 = (dP01du-dP00du)/eps;
  
  Vec3fa color = zero;
  switch (differentialMode)
  {
  case  0: color = dPdu0; break;
  case  1: color = dPdu1; break;
  case  2: color = 10.0f*(dPdu1-dPdu0); break;

  case  3: color = dPdv0; break;
  case  4: color = dPdv1; break;
  case  5: color = 10.0f*(dPdv1-dPdv0); break;

  case  6: color = ddPdudu0; break;
  case  7: color = ddPdudu1; break;
  case  8: color = 10.0f*(ddPdudu1-ddPdudu0); break;

  case  9: color = ddPdvdv0; break;
  case 10: color = ddPdvdv1; break;
  case 11: color = 10.0f*(ddPdvdv1-ddPdvdv0); break;

  case 12: color = ddPdudv0; break;
  case 13: color = ddPdudv1; break;
  case 14: color = 10.0f*(ddPdudv1-ddPdudv0); break;

  case 15: {
    color.x = length(dnormalize(cross(dPdu1,dPdv1),cross(ddPdudu1,dPdv1)+cross(dPdu1,ddPdudv1)))/length(dPdu1); 
    color.y = length(dnormalize(cross(dPdu1,dPdv1),cross(ddPdudv1,dPdv1)+cross(dPdu1,ddPdvdv1)))/length(dPdv1); 
    color.z = 0.0f;
    break;
  }
  case 16: {
    float Cu = length(dnormalize(cross(dPdu1,dPdv1),cross(ddPdudu1,dPdv1)+cross(dPdu1,ddPdudv1)))/length(dPdu1); 
    float Cv = length(dnormalize(cross(dPdu1,dPdv1),cross(ddPdudv1,dPdv1)+cross(dPdu1,ddPdvdv1)))/length(dPdv1); 
    color = Vec3fa(sqrt(Cu*Cu + Cv*Cv));
    break;
  }
  }
  color = color * pow(0.5f,10.0f*g_debug);
  return clamp(color,Vec3fa(zero),Vec3fa(one));
}

void renderTileDifferentials(int taskIndex, int* pixels, 
                             const unsigned int width, const unsigned int height, 
                             const float time, const ISPCCamera& camera,
                             const int numTilesX, const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y = y0; y<y1; y++) for (unsigned int x = x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelDifferentials(x,y,camera);

    /* write color to framebuffer */
    unsigned int r = (unsigned int) (255.0f * clamp(color.x,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(color.y,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(color.z,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;  
  }
}

/* returns the point seen through specified pixel */
extern "C" bool device_pick(const float x,
                            const float y, 
                            const ISPCCamera& camera,
                            Vec3fa& hitPos)
{
  /* initialize ray */
  RTCRay ray;
  ray.org = Vec3f(camera.xfm.p);
  ray.dir = Vec3f(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = g_debug;

  /* intersect ray with scene */
  rtcIntersect(g_scene,ray);
  PRINT2(x,y);
  PRINT(ray.geomID);
  PRINT(ray.primID);
  Vec3fa hit_point = ray.org + ray.tfar*ray.dir;
  PRINT(hit_point);
  PRINT(ray);

  /* shade pixel */
  if (ray.geomID == RTC_INVALID_GEOMETRY_ID) {
    hitPos = Vec3fa(0.0f,0.0f,0.0f);
    return false;
  }
  else {
    hitPos = ray.org + ray.tfar*ray.dir;
    return true;
  }
}

/* called when a key is pressed */
extern "C" void device_key_pressed_default(int key)
{
  if (key == GLUT_KEY_F1) {
    renderTile = renderTileStandard;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F2) {
    renderTile = renderTileEyeLight;
    g_changed = true;
  }    
  else if (key == GLUT_KEY_F3) {
    renderTile = renderTileWireframe;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F4) {
    renderTile = renderTileUV;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F5) {
    renderTile = renderTileNg;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F6) {
    renderTile = renderTileGeomID;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F7) {
    renderTile = renderTileGeomIDPrimID;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F8) {
    renderTile = renderTileUV16;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F9) {
    if (renderTile == renderTileCycles) scale *= 2.0f;
    PRINT(scale);
    renderTile = renderTileCycles;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F10) {
    if (renderTile == renderTileCycles) scale *= 0.5f;
    PRINT(scale);
    renderTile = renderTileCycles;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F11) {
    renderTile = renderTileAmbientOcclusion;
    g_changed = true;
  }
  else if (key == GLUT_KEY_F12) 
  {
    if (renderTile == renderTileDifferentials) {
      differentialMode = (differentialMode+1)%17;
    } else {
      renderTile = renderTileDifferentials;
      differentialMode = 0;
    }
    PRINT(differentialMode);
    g_changed = true;
  }
}

/* called when a key is pressed */
extern "C" 
{
  void (*key_pressed_handler)(int key) = nullptr;

  void call_key_pressed_handler(int key) {
    if (key_pressed_handler) key_pressed_handler(key);
  }
}

/* draws progress bar */
static int progressWidth = 0;
static std::atomic<size_t> progressDots(0);

void progressStart() 
{
  progressDots = 0;
  progressWidth = max(3,getTerminalWidth());
  std::cout << "[" << std::flush;
}

bool progressMonitor(void* ptr, const double n)
{
  size_t olddots = progressDots;
  size_t maxdots = progressWidth-2;
  size_t newdots = max(olddots,min(size_t(maxdots),size_t(n*double(maxdots))));
  if (progressDots.compare_exchange_strong(olddots,newdots))
    for (size_t i=olddots; i<newdots; i++) std::cout << "." << std::flush;
  return true;
}

void progressEnd() {
  std::cout << "]" << std::endl;
}

Vec2f getTextureCoordinatesSubdivMesh(void* _mesh, const unsigned int primID, const float u, const float v)
{
  ISPCSubdivMesh *mesh = (ISPCSubdivMesh *)_mesh;
  Vec2f st;
  st.x = u;
  st.y = v;
  if (mesh && mesh->texcoord_indices && mesh->texcoords)
    {
      assert(primID < mesh->numFaces);
      const size_t face_offset = mesh->face_offsets[primID];
      if (mesh->verticesPerFace[primID] == 3)
	{
	  const size_t t0 = mesh->texcoord_indices[face_offset+0];
	  const size_t t1 = mesh->texcoord_indices[face_offset+1];
	  const size_t t2 = mesh->texcoord_indices[face_offset+2];
	  const Vec2f &txt0 = mesh->texcoords[t0];
	  const Vec2f &txt1 = mesh->texcoords[t1];
	  const Vec2f &txt2 = mesh->texcoords[t2];
	  const float w = 1.0f - u - v;
	  st = w * txt0 + u * txt1 + v * txt2;
	}
      else if (mesh->verticesPerFace[primID] == 4)
	{
	  const size_t t0 = mesh->texcoord_indices[face_offset+0];
	  const size_t t1 = mesh->texcoord_indices[face_offset+1];
	  const size_t t2 = mesh->texcoord_indices[face_offset+2];
	  const size_t t3 = mesh->texcoord_indices[face_offset+3];
	  const Vec2f &txt0 = mesh->texcoords[t0];
	  const Vec2f &txt1 = mesh->texcoords[t1];
	  const Vec2f &txt2 = mesh->texcoords[t2];
	  const Vec2f &txt3 = mesh->texcoords[t3];
	  const float u0 = u;
	  const float v0 = v;
	  const float u1 = 1.0f - u;
	  const float v1 = 1.0f - v;
	  st = u1*v1 * txt0 + u0*v1* txt1 + u0*v0 * txt2 + u1*v0* txt3;	  
	}
#if defined(_DEBUG)
      else
	PRINT("not supported");
#endif
    }
  return st;
}

float getTextureTexel1f(const Texture* texture, float s, float t)
{
  if (texture == nullptr) 
    return 0.0f;

  int iu = (int)floorf(s * (float)(texture->width));
  iu = iu % texture->width; if (iu < 0) iu += texture->width;
  int iv = (int)floorf(t * (float)(texture->height));
  iv = iv % texture->height; if (iv < 0) iv += texture->height;
  
  if (likely(texture->format == Texture::FLOAT32))
  {
    float* data = (float*)texture->data;
    return data[iv*texture->width + iu];
  } 
  else if (likely(texture->format == Texture::RGBA8))
  {
    unsigned char* t = (unsigned char*)texture->data + (iv * texture->width + iu) * 4;
    return (float)t[0] * (1.0f/255.0f);
  }  
  return 0.0f;
}

Vec3f getTextureTexel3f(const Texture* texture, float s, float t)
{
  if (texture == nullptr)
    return Vec3f(0.0f);

  int iu = (int)floorf(s * (float)(texture->width));
  iu = iu % texture->width; if (iu < 0) iu += texture->width;
  int iv = (int)floorf(t * (float)(texture->height));
  iv = iv % texture->height; if (iv < 0) iv += texture->height;
  
   if (likely(texture->format == Texture::RGBA8)) {
     unsigned char *t = (unsigned char*)texture->data + (iv * texture->width + iu) * 4; //texture->bytesPerTexel;
     return Vec3f(  (float)t[0] * 1.0f/255.0f, (float)t[1] * 1.0f/255.0f, (float)t[2] * 1.0f/255.0f );
   }  
  return Vec3f(0.0f);;
}

#if defined(__cplusplus)
}
#endif
