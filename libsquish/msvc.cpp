#include "msvc.h"

#ifdef _MSC_VER

#include <cmath>

namespace std {
	float min(float a, float b) { return a < b ? a : b; }
	float max(float a, float b) { return a > b ? a : b; }
	float floor(float a) { return ::floor(a); }
	float ceil(float a) { return ::ceil(a); }
	float sqrt(float a) { return ::sqrt(a); }
	float fabs(float a) { return ::fabs(a); }
	float atan2(float a, float b) { return ::atan2(a, b); }
	float pow(float a, float b) { return ::pow(a, b); }
	float cos(float a) { return ::cos(a); }
	float sin(float a) { return ::sin(a); }
}
#endif

