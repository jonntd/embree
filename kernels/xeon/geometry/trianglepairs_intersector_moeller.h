// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
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

#pragma once

#include "triangle.h"
#include "trianglepairsv.h"
#include "trianglev_mb.h"
#include "intersector_epilog.h"
#include "triangle_intersector_moeller.h"

/*! This intersector implements a modified version of the Moeller
 *  Trumbore intersector from the paper "Fast, Minimum Storage
 *  Ray-Triangle Intersection". In contrast to the paper we
 *  precalculate some factors and factor the calculations differently
 *  to allow precalculating the cross product e1 x e2. The resulting
 *  algorithm is similar to the fastest one of the paper "Optimizing
 *  Ray-Triangle Intersection via Automated Search". */

namespace embree
{
  namespace isa
  {
#if 0
     template<int M>
        struct MoellerTrumboreIntersectorPairs1 : MoellerTrumboreIntersector1<M>
    {
      __forceinline MoellerTrumboreIntersectorPairs1(const Ray& ray, const void* ptr)
        : MoellerTrumboreIntersector1<M>(ray,ptr) {}

      template<typename Epilog>
        struct GetHit
        {
          const vint<M>& flags;
          const Epilog& epilog; 
          
          __forceinline GetHit(const vint<M>& flags, const Epilog& epilog) 
            : flags(flags), epilog(epilog) {}
          
          template<typename Hit>
          __forceinline bool operator() (const vbool<M>& valid_i, const Hit& hit) const 
          {
            return epilog(valid_i,[&] (const vbool<M>& valid) {
                vfloat<M> u, v, t; Vec3<vfloat<M>> Ng; size_t i; std::tie(u,v,t,Ng,i) = hit(valid);
                const vfloat<M> w = 1.0f - u - v;
                const vfloat<M> uwv[3] = { u,w,v };
                const unsigned int indexU = (((unsigned int)flags[i]) >>  0) & 0xff;
                const unsigned int indexV = (((unsigned int)flags[i]) >> 16) & 0xff;
                
                /* update hit information */
                const vfloat<M> uu = uwv[indexU];
                const vfloat<M> vv = uwv[indexV];
                return std::make_tuple(uu,vv,t,Ng,i);
              });
          }
        };
      
      template<typename Epilog>
        __forceinline bool intersect(Ray& ray, 
                                     const Vec3<vfloat<M>>& v0, 
                                     const Vec3<vfloat<M>>& v1, 
                                     const Vec3<vfloat<M>>& v2, 
                                     const vint<M>& flags,
                                     const Epilog& epilog) const
      {
        return MoellerTrumboreIntersector1<M>::intersect(ray,v0,v1,v2,GetHit<Epilog>(flags,epilog));
      }
    };
#endif

    template<int M>
      struct MoellerTrumboreIntersectorPairs1
    {
      struct Hit
      {
        __forceinline Hit(const vfloat<M>& u, const vfloat<M>& v, const vfloat<M>& t, const Vec3<vfloat<M>>& Ng, const vint<M>& flags)
          : vu(u), vv(v), vt(t), vNg(Ng), flags(flags) {}

        __forceinline Vec2f uv (const size_t i) 
        { 
          const vfloat<M> vw = 1.0f - vu - vv;
          const vfloat<M> uwv[3] = { vu,vw,vv };
          const unsigned int indexU = (((unsigned int)flags[i]) >>  0) & 0xff;
          const unsigned int indexV = (((unsigned int)flags[i]) >> 16) & 0xff;
          const float uu = uwv[indexU][i];
          const float vv = uwv[indexV][i];
          return Vec2f(uu,vv);
        }

        __forceinline float t  (const size_t i) { return vt[i]; }
        __forceinline Vec3fa Ng(const size_t i) { 
          return Vec3fa(vNg.x[i],vNg.y[i],vNg.z[i]); 
        }

      public:
        const vfloat<M> vu;
        const vfloat<M> vv;
        const vfloat<M> vt;
        const Vec3<vfloat<M>> vNg;
        const vint<M> flags;
      };

      __forceinline MoellerTrumboreIntersectorPairs1(const Ray& ray, const void* ptr) {}

      template<typename Epilog>
      __forceinline bool intersect(Ray& ray, 
                                   const Vec3<vfloat<M>>& tri_v0, 
                                   const Vec3<vfloat<M>>& tri_e1, 
                                   const Vec3<vfloat<M>>& tri_e2, 
                                   const Vec3<vfloat<M>>& tri_Ng,
                                   const vint<M>& flags,
                                   const Epilog& epilog) const
      {
        /* calculate denominator */
        typedef Vec3<vfloat<M>> Vec3vfM;
        const Vec3vfM O = Vec3vfM(ray.org);
        const Vec3vfM D = Vec3vfM(ray.dir);
        const Vec3vfM C = Vec3vfM(tri_v0) - O;
        const Vec3vfM R = cross(D,C);
        const vfloat<M> den = dot(Vec3vfM(tri_Ng),D);
        const vfloat<M> absDen = abs(den);
        const vfloat<M> sgnDen = signmsk(den);
        
        /* perform edge tests */
        const vfloat<M> U = dot(R,Vec3vfM(tri_e2)) ^ sgnDen;
        const vfloat<M> V = dot(R,Vec3vfM(tri_e1)) ^ sgnDen;
        
        /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
        vbool<M> valid = (den > vfloat<M>(zero)) & (U >= 0.0f) & (V >= 0.0f) & (U+V<=absDen);
#else
        vbool<M> valid = (den != vfloat<M>(zero)) & (U >= 0.0f) & (V >= 0.0f) & (U+V<=absDen);
#endif
        if (likely(none(valid))) return false;
        
        /* perform depth test */
        const vfloat<M> T = dot(Vec3vfM(tri_Ng),C) ^ sgnDen;
        valid &= (T > absDen*vfloat<M>(ray.tnear)) & (T < absDen*vfloat<M>(ray.tfar));
        if (likely(none(valid))) return false;

        /* update hit information */
        return epilog(valid,[&] () {
            const vfloat<M> rcpAbsDen = rcp(absDen);
            const vfloat<M> t = T * rcpAbsDen;
            const vfloat<M> u = U * rcpAbsDen;
            const vfloat<M> v = V * rcpAbsDen;
            const vfloat<M> flip(-1.0f,-1.0f,-1.0f,-1.0f,1.0f,1.0f,1.0f,1.0f);
            const Vec3vfM Ng(tri_Ng.x * flip,tri_Ng.y * flip,tri_Ng.z * flip);
            return Hit(u,v,t,Ng,flags);
          });
      }
      
      template<typename Epilog>
        __forceinline bool intersect(Ray& ray, 
                                     const Vec3<vfloat<M>>& v0, 
                                     const Vec3<vfloat<M>>& v1, 
                                     const Vec3<vfloat<M>>& v2, 
                                     const vint<M>& flags,
                                     const Epilog& epilog) const
      {
        const Vec3<vfloat<M>> e1 = v0-v1;
        const Vec3<vfloat<M>> e2 = v2-v0;
        const Vec3<vfloat<M>> Ng = cross(e1,e2);
        return intersect(ray,v0,e1,e2,Ng,flags,epilog);
      }
    };
      
    template<int M, int K>
    struct MoellerTrumboreIntersectorPairK
    {
       struct Hit
      {
        __forceinline Hit(const vfloat<M>& u, const vfloat<M>& v, const vfloat<M>& t, const Vec3<vfloat<M>>& Ng, const vint<M>& flags)
          : vu(u), vv(v), vt(t), vNg(Ng), flags(flags) {}

        __forceinline Vec2f uv (const size_t i) 
        { 
          const vfloat<M> vw = 1.0f - vu - vv;
          const vfloat<M> uwv[3] = { vu,vw,vv };
          const unsigned int indexU = (((unsigned int)flags[i]) >>  0) & 0xff;
          const unsigned int indexV = (((unsigned int)flags[i]) >> 16) & 0xff;
          const float uu = uwv[indexU][i];
          const float vv = uwv[indexV][i];
          return Vec2f(uu,vv);
        }

        __forceinline float t  (const size_t i) { return vt[i]; }
        __forceinline Vec3fa Ng(const size_t i) { return Vec3fa(vNg.x[i],vNg.y[i],vNg.z[i]); }

      public:
        const vfloat<M> vu;
        const vfloat<M> vv;
        const vfloat<M> vt;
        const Vec3<vfloat<M>> vNg;
        const vint<M> flags;
      };

      __forceinline MoellerTrumboreIntersectorPairK(const vbool<K>& valid, const RayK<K>& ray) {}
            
      /*! Intersects K rays with one of M triangles. */
      template<typename Epilog>
        __forceinline vbool<K> intersectK(const vbool<K>& valid0, 
                                          RayK<K>& ray, 
                                          const Vec3<vfloat<K>>& tri_v0, 
                                          const Vec3<vfloat<K>>& tri_e1, 
                                          const Vec3<vfloat<K>>& tri_e2, 
                                          const Vec3<vfloat<K>>& tri_Ng, 
                                          const unsigned int rotation,
                                          const float flipNg,
                                          const Epilog& epilog) const
      {
        /* ray SIMD type shortcuts */
        typedef Vec3<vfloat<K>> Vec3vfK;
        
        /* calculate denominator */
        vbool<K> valid = valid0;
        const Vec3vfK C = tri_v0 - ray.org;
        const Vec3vfK R = cross(ray.dir,C);
        const vfloat<K> den = dot(tri_Ng,ray.dir);
        const vfloat<K> absDen = abs(den);
        const vfloat<K> sgnDen = signmsk(den);
        
        /* test against edge p2 p0 */
        const vfloat<K> U = dot(R,tri_e2) ^ sgnDen;
        valid &= U >= 0.0f;
        if (likely(none(valid))) return false;
        
        /* test against edge p0 p1 */
        const vfloat<K> V = dot(R,tri_e1) ^ sgnDen;
        valid &= V >= 0.0f;
        if (likely(none(valid))) return false;
        
        /* test against edge p1 p2 */
        const vfloat<K> W = absDen-U-V;
        valid &= W >= 0.0f;
        if (likely(none(valid))) return false;
        
        /* perform depth test */
        const vfloat<K> T = dot(tri_Ng,C) ^ sgnDen;
        valid &= (T >= absDen*ray.tnear) & (absDen*ray.tfar >= T);
        if (unlikely(none(valid))) return false;
        
        /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
        valid &= den > vfloat<K>(zero);
        if (unlikely(none(valid))) return false;
#else
        valid &= den != vfloat<K>(zero);
        if (unlikely(none(valid))) return false;
#endif
        
        /* calculate hit information */
        return epilog(valid,[&] () {
            const vfloat<K> rcpAbsDen = rcp(absDen);
            const unsigned int indexU = (rotation >>  0) & 0xff;
            const unsigned int indexV = (rotation >> 16) & 0xff;
            const vfloat<K> t =  T*rcpAbsDen;
            const vfloat<K> uu = U*rcpAbsDen;
            const vfloat<K> vv = V*rcpAbsDen;
            const vfloat<K> ww = 1.0f-uu-vv;
            const vfloat<K> uwv[3] { uu,ww,vv };
            const vfloat<K> u = uwv[indexU];
            const vfloat<K> v = uwv[indexV];
            const vfloat<K> flip(flipNg); 
            const Vec3vfK Ng(tri_Ng.x*flip,tri_Ng.y*flip,tri_Ng.z*flip);
            return std::make_tuple(u,v,t,Ng);
          });
      }
      
      /*! Intersects K rays with one of M triangles. */
      template<typename Epilog>
      __forceinline vbool<K> intersectK(const vbool<K>& valid0, 
                                        RayK<K>& ray, 
                                        const Vec3<vfloat<K>>& tri_v0, 
                                        const Vec3<vfloat<K>>& tri_v1, 
                                        const Vec3<vfloat<K>>& tri_v2, 
                                        const unsigned int rotation,
                                        const float flipNg,
                                        const Epilog& epilog) const
      {
        typedef Vec3<vfloat<K>> Vec3vfK;
        const Vec3vfK e1 = tri_v0-tri_v1;
        const Vec3vfK e2 = tri_v2-tri_v0;
        const Vec3vfK Ng = cross(e1,e2);
        return intersectK(valid0,ray,tri_v0,e1,e2,Ng,rotation,flipNg,epilog);
      }
      
      /*! Intersect k'th ray from ray packet of size K with M triangles. */
      template<typename Epilog>
        __forceinline bool intersect(RayK<K>& ray, 
                                     size_t k,
                                     const Vec3<vfloat<M>>& tri_v0, 
                                     const Vec3<vfloat<M>>& tri_e1, 
                                     const Vec3<vfloat<M>>& tri_e2, 
                                     const Vec3<vfloat<M>>& tri_Ng,
                                     const vint<M>& flags,                                     
                                     const Epilog& epilog) const
      {
        /* calculate denominator */
        typedef Vec3<vfloat<M>> Vec3vfM;
        const Vec3vfM O = broadcast<vfloat<M>>(ray.org,k);
        const Vec3vfM D = broadcast<vfloat<M>>(ray.dir,k);
        const Vec3vfM C = Vec3vfM(tri_v0) - O;
        const Vec3vfM R = cross(D,C);
        const vfloat<M> den = dot(Vec3vfM(tri_Ng),D);
        const vfloat<M> absDen = abs(den);
        const vfloat<M> sgnDen = signmsk(den);
        
        /* perform edge tests */
        const vfloat<M> U = dot(R,Vec3vfM(tri_e2)) ^ sgnDen;
        const vfloat<M> V = dot(R,Vec3vfM(tri_e1)) ^ sgnDen;
        
        /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
        vbool<M> valid = (den > vfloat<M>(zero)) & (U >= 0.0f) & (V >= 0.0f) & (U+V<=absDen);
#else
        vbool<M> valid = (den != vfloat<M>(zero)) & (U >= 0.0f) & (V >= 0.0f) & (U+V<=absDen);
#endif
        if (likely(none(valid))) return false;
        
        /* perform depth test */
        const vfloat<M> T = dot(Vec3vfM(tri_Ng),C) ^ sgnDen;
        valid &= (T > absDen*vfloat<M>(ray.tnear[k])) & (T < absDen*vfloat<M>(ray.tfar[k]));
        if (likely(none(valid))) return false;
        
        /* calculate hit information */
        return epilog(valid,[&] () {
            const vfloat<M> rcpAbsDen = rcp(absDen);
            const vfloat<M> t = T * rcpAbsDen;
            const vfloat<M> u = U * rcpAbsDen;
            const vfloat<M> v = V * rcpAbsDen;
            const vfloat<M> flip(-1.0f,-1.0f,-1.0f,-1.0f,1.0f,1.0f,1.0f,1.0f);
            const Vec3vfM Ng(tri_Ng.x * flip,tri_Ng.y * flip,tri_Ng.z * flip);
            return Hit(u,v,t,Ng,flags);
          });
      }
      
      template<typename Epilog>
      __forceinline bool intersect1(RayK<K>& ray, 
                                    size_t k,
                                    const Vec3<vfloat<M>>& v0, 
                                    const Vec3<vfloat<M>>& v1, 
                                    const Vec3<vfloat<M>>& v2, 
                                    const vint<M>& flags,
                                    const Epilog& epilog) const
      {
        const Vec3<vfloat<M>> e1 = v0-v1;
        const Vec3<vfloat<M>> e2 = v2-v0;
        const Vec3<vfloat<M>> Ng = cross(e1,e2);
        return intersect(ray,k,v0,e1,e2,Ng,flags,epilog);
      }
    };


    /*! Intersects M triangle pairs with 1 ray */
    template<int M, bool filter>
      struct TrianglePairsMIntersector1MoellerTrumbore
      {
        typedef TrianglePairsMv<M> Primitive;
#if defined(__AVX__)
        typedef MoellerTrumboreIntersectorPairs1<2*M> Precalculations;
#else
        typedef MoellerTrumboreIntersectorPairs1<M> Precalculations;
#endif
        
        /*! Intersect a ray with the M triangles and updates the hit. */
        static __forceinline void intersect(const Precalculations& pre, Ray& ray, const Primitive& tri, Scene* scene, const unsigned* geomID_to_instID)
        {
          STAT3(normal.trav_prims,1,1,1);
#if defined(__AVX__)
        Vec3vf8 vtx0(vfloat8(tri.v1.x,tri.v3.x),
                     vfloat8(tri.v1.y,tri.v3.y),
                     vfloat8(tri.v1.z,tri.v3.z));
        Vec3vf8 vtx1(vfloat8(tri.v0.x),
                     vfloat8(tri.v0.y),
                     vfloat8(tri.v0.z));
        Vec3vf8 vtx2(vfloat8(tri.v2.x),
                     vfloat8(tri.v2.y),
                     vfloat8(tri.v2.z));
        vint8   geomIDs(tri.geomIDs); 
        vint8   primIDs(tri.primIDs,tri.primIDs+1);
        vint8   flags(tri.flags);
        pre.intersect(ray,vtx0,vtx1,vtx2,flags,Intersect1Epilog<2*M,filter>(ray,geomIDs,primIDs,scene,geomID_to_instID));          
#else
        FATAL("SSE mode not yet supported");
        //vint<M> geomIDs(tri.geomIDs);
        //pre.intersect(ray,tri.v1,tri.v0,tri.v2,IntersectPairs1Epilog<M,filter>(ray,geomIDs,tri.primIDs+0,scene,geomID_to_instID));
        //pre.intersect(ray,tri.v3,tri.v0,tri.v2,IntersectPairs1Epilog<M,filter>(ray,geomIDs,tri.primIDs+1,scene,geomID_to_instID));
#endif
        }
        
        /*! Test if the ray is occluded by one of M triangles. */
        static __forceinline bool occluded(const Precalculations& pre, Ray& ray, const Primitive& tri, Scene* scene, const unsigned* geomID_to_instID)
        {
          STAT3(shadow.trav_prims,1,1,1);
#if defined(__AVX__)
          Vec3vf8 vtx0(vfloat8(tri.v1.x,tri.v3.x),
                       vfloat8(tri.v1.y,tri.v3.y),
                       vfloat8(tri.v1.z,tri.v3.z));
          Vec3vf8 vtx1(vfloat8(tri.v0.x),
                       vfloat8(tri.v0.y),
                       vfloat8(tri.v0.z));
          Vec3vf8 vtx2(vfloat8(tri.v2.x),
                       vfloat8(tri.v2.y),
                       vfloat8(tri.v2.z));
          vint8   geomIDs(tri.geomIDs); 
          vint8   primIDs(tri.primIDs,tri.primIDs+1);
          vint8   flags(tri.flags);
          return pre.intersect(ray,vtx0,vtx1,vtx2,flags,Occluded1Epilog<2*M,filter>(ray,geomIDs,primIDs,scene,geomID_to_instID));
#else
          FATAL("SSE mode not yet supported");
          //vint<M> geomIDs(tri.geomIDs);
          //if (pre.intersect(ray,tri.v0,tri.v1,tri.v2,OccludedPairs1Epilog<M,filter>(ray,geomIDs,tri.primIDs+0,scene,geomID_to_instID))) return true;
          //if (pre.intersect(ray,tri.v0,tri.v2,tri.v3,OccludedPairs1Epilog<M,filter>(ray,geomIDs,tri.primIDs+1,scene,geomID_to_instID))) return true;
          
#endif
          return false;
        }
      };


    /*! Intersects M triangles with K rays. */
    template<int M, int K, bool filter>
      struct TrianglePairsMIntersectorKMoellerTrumbore
      {
        typedef TrianglePairsMv<M> Primitive;
        typedef MoellerTrumboreIntersectorPairK<2*M,K> Precalculations;
        
        /*! Intersects K rays with M triangles. */
        static __forceinline void intersect(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, const TrianglePairsMv<M>& tri, Scene* scene)
        {
          for (size_t i=0; i<TrianglePairsMv<M>::max_size(); i++)
          {
            if (!tri.valid(i)) break;
            STAT3(normal.trav_prims,1,popcnt(valid_i),K);
            const Vec3<vfloat<K>> p0 = broadcast<vfloat<K>>(tri.v0,i);
            const Vec3<vfloat<K>> p1 = broadcast<vfloat<K>>(tri.v1,i);
            const Vec3<vfloat<K>> p2 = broadcast<vfloat<K>>(tri.v2,i);
            const unsigned int rotation0 = tri.flags[i];
            pre.intersectK(valid_i,ray,p1,p0,p2,rotation0,-1.0f,IntersectKEpilog<M,K,filter>(ray,tri.geomIDs,tri.primIDs  ,i,scene));
            const Vec3<vfloat<K>> p3 = broadcast<vfloat<K>>(tri.v3,i);
            const unsigned int rotation1 = tri.flags[4+i];
            pre.intersectK(valid_i,ray,p3,p0,p2,rotation1,1.0f,IntersectKEpilog<M,K,filter>(ray,tri.geomIDs,tri.primIDs+1,i,scene));
          }
        }
        
        /*! Test for K rays if they are occluded by any of the M triangles. */
        static __forceinline vbool<K> occluded(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, const TrianglePairsMv<M>& tri, Scene* scene)
        {
          vbool<K> valid0 = valid_i;
          
          for (size_t i=0; i<TrianglePairsMv<M>::max_size(); i++)
          {
            if (!tri.valid(i)) break;
            STAT3(shadow.trav_prims,1,popcnt(valid0),K);
            const Vec3<vfloat<K>> p0 = broadcast<vfloat<K>>(tri.v0,i);
            const Vec3<vfloat<K>> p1 = broadcast<vfloat<K>>(tri.v1,i);
            const Vec3<vfloat<K>> p2 = broadcast<vfloat<K>>(tri.v2,i);
            const unsigned int rotation0 = tri.flags[i];
            pre.intersectK(valid0,ray,p0,p1,p2,rotation0,-1.0f,OccludedKEpilog<M,K,filter>(valid0,ray,tri.geomIDs,tri.primIDs,i,scene));
            if (none(valid0)) break;
            const Vec3<vfloat<K>> p3 = broadcast<vfloat<K>>(tri.v3,i);
            const unsigned int rotation1 = tri.flags[4+i];
            pre.intersectK(valid0,ray,p0,p2,p3,rotation1,1.0f,OccludedKEpilog<M,K,filter>(valid0,ray,tri.geomIDs,tri.primIDs,i,scene));
            if (none(valid0)) break;
          }
          return !valid0;
        }
        
        /*! Intersect a ray with M triangles and updates the hit. */
        static __forceinline void intersect(Precalculations& pre, RayK<K>& ray, size_t k, const TrianglePairsMv<M>& tri, Scene* scene)
        {
          STAT3(normal.trav_prims,1,1,1);
#if defined(__AVX__)
          Vec3vf8 vtx0(vfloat8(tri.v1.x,tri.v3.x),
                       vfloat8(tri.v1.y,tri.v3.y),
                       vfloat8(tri.v1.z,tri.v3.z));
          Vec3vf8 vtx1(vfloat8(tri.v0.x),
                       vfloat8(tri.v0.y),
                       vfloat8(tri.v0.z));
          Vec3vf8 vtx2(vfloat8(tri.v2.x),
                       vfloat8(tri.v2.y),
                       vfloat8(tri.v2.z));
          vint8   geomIDs(tri.geomIDs); 
          vint8   primIDs(tri.primIDs,tri.primIDs+1);
          vint8   flags(tri.flags);
          pre.intersect1(ray,k,vtx0,vtx1,vtx2,flags,Intersect1KEpilog<2*M,K,filter>(ray,k,geomIDs,primIDs,scene));
#else
          FATAL("SSE mode not supported");
          //pre.intersect(ray,k,tri.v0,tri.v1,tri.v2,Intersect1KEpilog<M,K,filter>(ray,k,tri.geomIDs,tri.primIDs,scene));
          //pre.intersect(ray,k,tri.v0,tri.v2,tri.v3,Intersect1KEpilog<M,K,filter>(ray,k,tri.geomIDs,tri.primIDs,scene));
#endif
        }
        
        /*! Test if the ray is occluded by one of the M triangles. */
        static __forceinline bool occluded(Precalculations& pre, RayK<K>& ray, size_t k, const TrianglePairsMv<M>& tri, Scene* scene)
        {
          STAT3(shadow.trav_prims,1,1,1);
#if defined(__AVX__)
          Vec3vf8 vtx0(vfloat8(tri.v1.x,tri.v3.x),
                       vfloat8(tri.v1.y,tri.v3.y),
                       vfloat8(tri.v1.z,tri.v3.z));
          Vec3vf8 vtx1(vfloat8(tri.v0.x),
                       vfloat8(tri.v0.y),
                       vfloat8(tri.v0.z));
          Vec3vf8 vtx2(vfloat8(tri.v2.x),
                       vfloat8(tri.v2.y),
                       vfloat8(tri.v2.z));
          vint8   geomIDs(tri.geomIDs); 
          vint8   primIDs(tri.primIDs,tri.primIDs+1);
          vint8   flags(tri.flags);
          return pre.intersect1(ray,k,vtx0,vtx1,vtx2,flags,Occluded1KEpilog<2*M,K,filter>(ray,k,geomIDs,primIDs,scene));          
#else
          FATAL("SSE mode not supported");
          //if (pre.intersect(ray,k,tri.v0,tri.v1,tri.v2,Occluded1KEpilog<M,K,filter>(ray,k,tri.geomIDs,tri.primIDs,scene))) return true;
          //if (pre.intersect(ray,k,tri.v0,tri.v2,tri.v3,Occluded1KEpilog<M,K,filter>(ray,k,tri.geomIDs,tri.primIDs,scene))) return true;
#endif
          return false;
        }
      };


  }
}