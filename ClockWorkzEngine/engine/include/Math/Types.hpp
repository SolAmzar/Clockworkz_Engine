/******************************************************************************/
/*!
\file           Types.hpp
\project        Lights Out
\author(s)      [Amzar]
\brief          Establishes fixed-size integer and floating-point types for cross-platform consistency.
				Responsibilities include:
				- Defining standard-width integer types (s8, u8, s16, u16, s32, u32, s64, u64)
				- Defining floating-point types (f32 for float, f64 for double)
				- Providing a NULL definition if not already defined
				- Ensuring consistent type sizes across different compilers and platforms

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/


#pragma once
#ifndef TYPES
#define TYPES	

#include <cstdint>
typedef int8_t				s8;
typedef uint8_t     		u8;
typedef int16_t     		s16;
typedef uint16_t    		u16;
typedef int32_t             s32;
typedef uint32_t            u32;
typedef int64_t         	s64;
typedef uint64_t        	u64;
typedef float				f32;
typedef double				f64;

#ifndef NULL
# define NULL 0
#endif

#endif