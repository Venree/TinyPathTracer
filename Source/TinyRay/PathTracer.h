#pragma once
/*---------------------------------------------------------------------
*
* Copyright © 2015  Minsi Chen
* E-mail: m.chen@derby.ac.uk
*
* The source is written for the Graphics I and II modules. You are free
* to use and extend the functionality. The code provided here is functional
* however the author does not guarantee its performance.
---------------------------------------------------------------------*/
#pragma once

#include "Material.h"
#include "Ray.h"
#include "Scene.h"
#include "Framebuffer.h"

class PathTracer
{
private:
	Framebuffer		*m_framebuffer;
	int				m_buffWidth;
	int				m_buffHeight;
	int				m_renderCount;
	int				m_traceLevel;

	//Trace the scene from a given ray and scene
	//Params:
	//	Scene* pScene		pointer to the scene being traced
	//  Ray& ray			reference to an input ray
	//  Colour incolour		default colour to use when the ray does not intersect with any objects
	//  int tracelevel		the current recursion level of the TraceScene call
	//  bool shadowray		true if the input ray is a shadow ray, could be useful when handling shadows
	Colour Radiance(Scene* pScene, Ray& ray, int tracelevel, bool shadowray = false);

	//Compute lighting for a given ray-primitive intersection result
	//Params:
	//			std::vector<Light*>* lights     pointer to a list of active light sources
	//			Vector3*	pointer to the active camera
	//			RayHitResult* hitresult		Hit result from ray-primitive intersection
	//Colour Radiance(RayHitResult* hitresult, int depth);

public:

	enum TraceFlags
	{
		TRACE_PATH = 0
	};

	TraceFlags m_traceflag;						//current trace flags value default is TRACE_AMBIENT

	PathTracer();
	PathTracer(int width, int height);
	~PathTracer();

	inline void SetTraceLevel(int level)		//Set the level of recursion, default is 5
	{
		m_traceLevel = level;
	}

	inline void ResetRenderCount()
	{
		m_renderCount = 0;
	}

	inline Framebuffer *GetFramebuffer() const
	{
		return m_framebuffer;
	}

	//Trace a given scene
	//Params: Scene* pScene   Pointer to the scene to be ray traced
	void DoPathTrace(Scene* pScene);
};

