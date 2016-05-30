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

#include "transport_host.h"
#include "transport_device.h"
#include "../../common/tutorial/scene.h"
#include "../../common/tutorial/tutorial_device.h"
#include "../../common/tutorial/scene_device.h"
#include "../../../include/embree2/rtcore.h"

namespace embree
{
  extern "C" int64_t get_tsc() {
    return read_tsc();
  }
  
  /* framebuffer */
  int* g_pixels = nullptr;
  int g_width = -1;
  int g_height = -1;

  /* scene */
  extern "C" {
    ISPCScene* g_ispc_scene = nullptr;
  }

  void init(const char* cfg) {
    device_init(cfg);
  }

  void set_parameter(size_t parm, ssize_t val) {
    rtcDeviceSetParameter1i(nullptr,(RTCParameter)parm,val);
  }

  void key_pressed (int key) {
    call_key_pressed_handler(key);
  }

  void resize(int width, int height)
  {
    if (width == g_width && height == g_height) 
      return;

    if (g_pixels) alignedFree(g_pixels);
    g_width = width;
    g_height = height;
    g_pixels = (int*) alignedMalloc(g_width*g_height*sizeof(int),64);
  }

  void set_scene (TutorialScene* in) {
    g_ispc_scene = new ISPCScene(in);
  }

  bool pick(const float x, const float y, const ISPCCamera& camera, Vec3fa& hitPos) {
    return device_pick(x,y,camera,hitPos);
  }

  void render(const float time, const ISPCCamera& camera) {
    device_render(g_pixels,g_width,g_height,time,camera);
  }

  int* map () {
    return g_pixels;
  }
  
  void unmap () {
  }

  void cleanup()
  {
    device_cleanup();
    alignedFree(g_pixels); 
    g_pixels = nullptr;
    g_width = -1;
    g_height = -1;
  }
}
