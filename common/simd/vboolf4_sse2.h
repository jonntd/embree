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

#pragma once

namespace embree
{
  /* 4-wide SSE bool type */
  template<>
  struct vboolf<4>
  {
    typedef vboolf4 Bool;
    typedef vint4   Int;
    typedef vfloat4 Float;

    enum  { size = 4 };            // number of SIMD elements
    union { __m128 v; int i[4]; }; // data

    ////////////////////////////////////////////////////////////////////////////////
    /// Constructors, Assignment & Cast Operators
    ////////////////////////////////////////////////////////////////////////////////
    
    __forceinline vboolf            ( ) {}
    __forceinline vboolf            ( const vboolf4& other ) { v = other.v; }
    __forceinline vboolf4& operator=( const vboolf4& other ) { v = other.v; return *this; }

    __forceinline vboolf( const __m128  input ) : v(input) {}
    __forceinline operator const __m128&( void ) const { return v; }
    __forceinline operator const __m128i( void ) const { return _mm_castps_si128(v); }
    __forceinline operator const __m128d( void ) const { return _mm_castps_pd(v); }
    
    __forceinline vboolf( bool a )
      : v(_mm_lookupmask_ps[(size_t(a) << 3) | (size_t(a) << 2) | (size_t(a) << 1) | size_t(a)]) {}
    __forceinline vboolf( bool a, bool b )
      : v(_mm_lookupmask_ps[(size_t(b) << 3) | (size_t(a) << 2) | (size_t(b) << 1) | size_t(a)]) {}
    __forceinline vboolf( bool a, bool b, bool c, bool d )
      : v(_mm_lookupmask_ps[(size_t(d) << 3) | (size_t(c) << 2) | (size_t(b) << 1) | size_t(a)]) {}
    __forceinline vboolf( int mask ) {
      assert(mask >= 0 && mask < 16);
      v = _mm_lookupmask_ps[mask];
    }

    /* return int32 mask */
    __forceinline __m128i mask32() const { 
      return _mm_castps_si128(v);
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Constants
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline vboolf( FalseTy ) : v(_mm_setzero_ps()) {}
    __forceinline vboolf( TrueTy  ) : v(_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()))) {}

    ////////////////////////////////////////////////////////////////////////////////
    /// Array Access
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline bool operator []( const size_t index ) const { assert(index < 4); return (_mm_movemask_ps(v) >> index) & 1; }
    __forceinline int& operator []( const size_t index )       { assert(index < 4); return i[index]; }
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// Unary Operators
  ////////////////////////////////////////////////////////////////////////////////
  
  __forceinline const vboolf4 operator !( const vboolf4& a ) { return _mm_xor_ps(a, vboolf4(embree::True)); }
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Binary Operators
  ////////////////////////////////////////////////////////////////////////////////
  
  __forceinline const vboolf4 operator &( const vboolf4& a, const vboolf4& b ) { return _mm_and_ps(a, b); }
  __forceinline const vboolf4 operator |( const vboolf4& a, const vboolf4& b ) { return _mm_or_ps (a, b); }
  __forceinline const vboolf4 operator ^( const vboolf4& a, const vboolf4& b ) { return _mm_xor_ps(a, b); }
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Assignment Operators
  ////////////////////////////////////////////////////////////////////////////////
  
  __forceinline const vboolf4 operator &=( vboolf4& a, const vboolf4& b ) { return a = a & b; }
  __forceinline const vboolf4 operator |=( vboolf4& a, const vboolf4& b ) { return a = a | b; }
  __forceinline const vboolf4 operator ^=( vboolf4& a, const vboolf4& b ) { return a = a ^ b; }
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Comparison Operators + Select
  ////////////////////////////////////////////////////////////////////////////////
  
  __forceinline const vboolf4 operator !=( const vboolf4& a, const vboolf4& b ) { return _mm_xor_ps(a, b); }
  __forceinline const vboolf4 operator ==( const vboolf4& a, const vboolf4& b ) { return _mm_castsi128_ps(_mm_cmpeq_epi32(a, b)); }
  
  __forceinline const vboolf4 select( const vboolf4& m, const vboolf4& t, const vboolf4& f ) {
#if defined(__SSE4_1__)
    return _mm_blendv_ps(f, t, m); 
#else
    return _mm_or_ps(_mm_and_ps(m, t), _mm_andnot_ps(m, f)); 
#endif
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Movement/Shifting/Shuffling Functions
  ////////////////////////////////////////////////////////////////////////////////
  
  __forceinline const vboolf4 unpacklo( const vboolf4& a, const vboolf4& b ) { return _mm_unpacklo_ps(a, b); }
  __forceinline const vboolf4 unpackhi( const vboolf4& a, const vboolf4& b ) { return _mm_unpackhi_ps(a, b); }

  template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const vboolf4 shuffle( const vboolf4& a ) {
    return _mm_shuffle_epi32(a, _MM_SHUFFLE(i3, i2, i1, i0));
  }

  template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const vboolf4 shuffle( const vboolf4& a, const vboolf4& b ) {
    return _mm_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0));
  }

  template<size_t i0> __forceinline const vboolf4 shuffle( const vboolf4& b ) {
    return shuffle<i0,i0,i0,i0>(b);
  }

#if defined(__SSE3__)
  template<> __forceinline const vboolf4 shuffle<0, 0, 2, 2>( const vboolf4& a ) { return _mm_moveldup_ps(a); }
  template<> __forceinline const vboolf4 shuffle<1, 1, 3, 3>( const vboolf4& a ) { return _mm_movehdup_ps(a); }
  template<> __forceinline const vboolf4 shuffle<0, 1, 0, 1>( const vboolf4& a ) { return _mm_castpd_ps(_mm_movedup_pd (a)); }
#endif

#if defined(__SSE4_1__)
  template<size_t dst, size_t src, size_t clr> __forceinline const vboolf4 insert( const vboolf4& a, const vboolf4& b ) { return _mm_insert_ps(a, b, (dst << 4) | (src << 6) | clr); }
  template<size_t dst, size_t src> __forceinline const vboolf4 insert( const vboolf4& a, const vboolf4& b ) { return insert<dst, src, 0>(a, b); }
  template<size_t dst>             __forceinline const vboolf4 insert( const vboolf4& a, const bool b ) { return insert<dst,0>(a, vboolf4(b)); }
#endif
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Reduction Operations
  ////////////////////////////////////////////////////////////////////////////////
    
  __forceinline bool reduce_and( const vboolf4& a ) { return _mm_movemask_ps(a) == 0xf; }
  __forceinline bool reduce_or ( const vboolf4& a ) { return _mm_movemask_ps(a) != 0x0; }

  __forceinline bool all       ( const vboolf4& b ) { return _mm_movemask_ps(b) == 0xf; }
  __forceinline bool any       ( const vboolf4& b ) { return _mm_movemask_ps(b) != 0x0; }
  __forceinline bool none      ( const vboolf4& b ) { return _mm_movemask_ps(b) == 0x0; }

  __forceinline bool all       ( const vboolf4& valid, const vboolf4& b ) { return all(!valid | b); }
  __forceinline bool any       ( const vboolf4& valid, const vboolf4& b ) { return any( valid & b); }
  __forceinline bool none      ( const vboolf4& valid, const vboolf4& b ) { return none(valid & b); }
  
  __forceinline size_t movemask( const vboolf4& a ) { return _mm_movemask_ps(a); }
#if defined(__SSE4_2__)
  __forceinline size_t popcnt( const vboolf4& a ) { return __popcnt((size_t)_mm_movemask_ps(a)); }
#else
  __forceinline size_t popcnt( const vboolf4& a ) { return bool(a[0])+bool(a[1])+bool(a[2])+bool(a[3]); }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  /// Get/Set Functions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline bool get(const vboolf4& a, size_t index) { return a[index]; }
  __forceinline void set(vboolf4& a, size_t index)       { a[index] = -1; }
  __forceinline void clear(vboolf4& a, size_t index)     { a[index] =  0; }

  ////////////////////////////////////////////////////////////////////////////////
  /// Output Operators
  ////////////////////////////////////////////////////////////////////////////////
  
  inline std::ostream& operator<<(std::ostream& cout, const vboolf4& a) {
    return cout << "<" << a[0] << ", " << a[1] << ", " << a[2] << ", " << a[3] << ">";
  }
}
