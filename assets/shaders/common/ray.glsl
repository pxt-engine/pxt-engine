#ifndef _RAY_
#define _RAY_

// Minimum intersection distance
#define RAY_T_MIN 0.01

// Maximum intersection distance
#define RAY_T_MAX 100.0

struct Ray {
	vec3 origin;
	vec3 direction;
};

#endif