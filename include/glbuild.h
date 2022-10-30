// OpenGL interface declaration
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __glbuild_h__
#define __glbuild_h__

#ifdef __cplusplus
extern "C" {
#endif

#if USE_OPENGL
struct glbuild_info {
	int loaded;

	int majver;	// GL version
	int minver;
	int glslmajver;	// 0 = no support
	int glslminver;

	float maxanisotropy;
	char bgra;
	char clamptoedge;
	char texcomprdxt1;
	char texcomprdxt5;
	char texcompretc1;
	char texnpot;
	char multisample;
	char nvmultisamplehint;
	char sampleshading;

	int multitex;
	int maxtexsize;
	int maxvertexattribs;
	char debugext;
};
extern struct glbuild_info glinfo;
#endif

#ifdef __cplusplus
}
#endif

#endif // __glbuild_h__
