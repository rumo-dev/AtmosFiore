#ifndef __SAMPLERS_HLSL__
#define __SAMPLERS_HLSL__

//#include "Engine/System/samplers.h"

#define POINT_WRAP 0
#define LINEAR_WRAP 1
#define ANISOTROPIC_WRAP 2

#define POINT_CLAMP 3
#define LINEAR_CLAMP 4
#define ANISOTROPIC_CLAMP 5

#define POINT_BORDER_OPAQUE_BLACK 6
#define LINEAR_BORDER_OPAQUE_BLACK 7

#define POINT_BORDER_OPAQUE_WHITE 8
#define LINEAR_BORDER_OPAQUE_WHITE 9

#define POINT_MIRROR 10
#define LINEAR_MIRROR 11
#define ANISOTROPIC_MIRROR 12

#define COMPARISON_DEPTH 13
SamplerState sampler_states[13] : register(s0);
SamplerComparisonState comparison_sampler_state : register(s13);

#endif // __SAMPLERS_HLSL__