#ifndef libsquish_msvc_h
#define libsquish_msvc_h

#ifdef _MSC_VER

namespace std {
	float min(float a, float b);
	float max(float a, float b);
	float floor(float a);
	float ceil(float a);
	float sqrt(float a);
	float fabs(float a);
	float atan2(float a, float b);
	float pow(float a, float b);
	float cos(float a);
	float sin(float a);
}
#endif

#endif
