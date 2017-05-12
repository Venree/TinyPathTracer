/*---------------------------------------------------------------------
*
* Copyright © 2015  Minsi Chen
* E-mail: m.chen@derby.ac.uk
*
* The source is written for the Graphics I and II modules. You are free
* to use and extend the functionality. The code provided here is functional
* however the author does not guarantee its performance.
---------------------------------------------------------------------*/
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


#if defined(WIN32) || defined(_WINDOWS)
#include <Windows.h>
#include <gl/GL.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#endif

#include "RayTracer.h"
#include "Ray.h"
#include "Scene.h"
#include "Camera.h"
#include "perlin.h"


RayTracer::RayTracer()
{
	m_buffHeight = m_buffWidth = 0.0;
	m_renderCount = 0;
	SetTraceLevel(5);
	m_traceflag = (TraceFlags)(TRACE_AMBIENT | TRACE_DIFFUSE_AND_SPEC |
		TRACE_SHADOW | TRACE_REFLECTION | TRACE_REFRACTION);

}

RayTracer::RayTracer(int Width, int Height)
{
	m_buffWidth = Width;
	m_buffHeight = Height;
	m_renderCount = 0;
	SetTraceLevel(5);

	m_framebuffer = new Framebuffer(Width, Height);

	//default set default trace flag, i.e. no lighting, non-recursive
	m_traceflag = (TraceFlags)(TRACE_AMBIENT);
}

RayTracer::~RayTracer()
{
	delete m_framebuffer;
}

void RayTracer::DoRayTrace( Scene* pScene )
{
	Camera* cam = pScene->GetSceneCamera();
	
	Vector3 camRightVector = cam->GetRightVector();
	Vector3 camUpVector = cam->GetUpVector();
	Vector3 camViewVector = cam->GetViewVector();
	Vector3 centre = cam->GetViewCentre();
	Vector3 camPosition = cam->GetPosition();

	double sceneWidth = pScene->GetSceneWidth();
	double sceneHeight = pScene->GetSceneHeight();

	double pixelDX = sceneWidth / m_buffWidth;
	double pixelDY = sceneHeight / m_buffHeight;
	
	int total = m_buffHeight*m_buffWidth;
	int done_count = 0;
	
	Vector3 start;

	start[0] = centre[0] - ((sceneWidth * camRightVector[0])
		+ (sceneHeight * camUpVector[0])) / 2.0;
	start[1] = centre[1] - ((sceneWidth * camRightVector[1])
		+ (sceneHeight * camUpVector[1])) / 2.0;
	start[2] = centre[2] - ((sceneWidth * camRightVector[2])
		+ (sceneHeight * camUpVector[2])) / 2.0;
	
	Colour scenebg = pScene->GetBackgroundColour();

	if (m_renderCount == 0)
	{
		fprintf(stdout, "Trace start.\n");

		Colour colour;
//TinyRay on multiprocessors using OpenMP!!!
#pragma omp parallel for schedule (dynamic, 1) private(colour)
		for (int i = 0; i < m_buffHeight; i+=1) {
			for (int j = 0; j < m_buffWidth; j+=1) {

				//calculate the metric size of a pixel in the view plane (e.g. framebuffer)
				Vector3 pixel;

				pixel[0] = start[0] + (i + 0.5) * camUpVector[0] * pixelDY
					+ (j + 0.5) * camRightVector[0] * pixelDX;
				pixel[1] = start[1] + (i + 0.5) * camUpVector[1] * pixelDY
					+ (j + 0.5) * camRightVector[1] * pixelDX;
				pixel[2] = start[2] + (i + 0.5) * camUpVector[2] * pixelDY
					+ (j + 0.5) * camRightVector[2] * pixelDX;

				/*
				* setup first generation view ray
				* In perspective projection, each view ray originates from the eye (camera) position 
				* and pierces through a pixel in the view plane
				*/
				Ray viewray;
				viewray.SetRay(camPosition,	(pixel - camPosition).Normalise());
				
				double u = (double)j / (double)m_buffWidth;
				double v = (double)i / (double)m_buffHeight;

				scenebg = pScene->GetBackgroundColour();

				//trace the scene using the view ray
				//default colour is the background colour, unless something is hit along the way
				colour = this->TraceScene(pScene, viewray, scenebg, m_traceLevel);

				/*
				* Draw the pixel as a coloured rectangle
				*/
				m_framebuffer->WriteRGBToFramebuffer(colour, j, i);
			}
		}

		fprintf(stdout, "Done!!!\n");
		m_renderCount++;
	}
}

Colour RayTracer::TraceScene(Scene* pScene, Ray& ray, Colour incolour, int tracelevel, bool shadowray)
{
	RayHitResult result;

	Colour outcolour = incolour; //the output colour based on the ray-primitive intersection	

	std::vector<Light*> *light_list = pScene->GetLightList();
	std::vector<Light*>::iterator lit_iter = light_list->begin();
	Vector3 cameraPosition = pScene->GetSceneCamera()->GetPosition();

	result = pScene->IntersectByRay(ray);

	if (result.data) //the ray has hit something
	{
		outcolour = CalculateLighting(light_list, &cameraPosition, &result);

		//TODO: 2. The following conditions are for implementing recursive ray tracing
		if (m_traceflag & TRACE_REFLECTION)
		{
			//stops the walls/floor from reflecting
			if (((Primitive*)result.data)->m_primtype != Primitive::PRIMTYPE_Plane)
			{
				const double err = 1e-5;

				Ray reflectRay;
				//err adds a small offset to the vectors to avoid noise issues
				reflectRay.SetRay(result.point + (result.normal * err), ray.GetRay().Reflect(result.normal));

				if (tracelevel >= 1)
				{
					tracelevel--;
					outcolour = outcolour * TraceScene(pScene, reflectRay, outcolour, tracelevel, shadowray);
				}
			}
		}

		if (m_traceflag & TRACE_REFRACTION)
		{
			//TODO: trace the refraction ray from the intersection point

			//stops the walls/floor from reflecting
			if (((Primitive*)result.data)->m_primtype != Primitive::PRIMTYPE_Plane)
			{

				Vector3 norm = result.normal;
				Vector3 refractRay = ray.GetRay();

				float refractIndex = 1.33;
				double err = 4.001; 
				double dotProd = norm.DotProduct(refractRay);

				//clamp dotProd
				if (dotProd > 1)
				{
					dotProd = 1;
				}
				if (dotProd < 0)
				{
					dotProd = 0;
				}

				double index = dotProd / refractIndex;

				ray.SetRay((result.point + result.normal * -err), (ray.GetRay().Refract(result.normal, index)));

				if (tracelevel >= 1)
				{
					tracelevel--;
					outcolour = outcolour + TraceScene(pScene, ray, outcolour, tracelevel, shadowray);
				}
			}

		}

		if (m_traceflag & TRACE_SHADOW)
		{
			//TODO: trace the shadow ray from the intersection point	
			Ray shadow_ray;

			Vector3 lightPosition = (*lit_iter)->GetLightPosition();

			const double err = 1e-5;
			shadow_ray.SetRay(result.point + result.normal * err, (lightPosition - result.point).Normalise());
			RayHitResult hit = pScene->IntersectByRay(shadow_ray);
			Colour shadowColour = outcolour * 0.50;

			if (hit.data && ((Primitive*)hit.data)->m_primtype != Primitive::PRIMTYPE_Plane)
			{
				outcolour = shadowColour;
			}
		}
	}
		
	return outcolour;
}

Colour RayTracer::CalculateLighting(std::vector<Light*>* lights, Vector3* campos, RayHitResult* hitresult)
{
	Colour outcolour;
	std::vector<Light*>::iterator lit_iter = lights->begin();

	Primitive* prim = (Primitive*)hitresult->data;
	Material* mat = prim->GetMaterial();

	outcolour = mat->GetAmbientColour();
	
	//Generate the grid pattern on the plane
	if (((Primitive*)hitresult->data)->m_primtype == Primitive::PRIMTYPE_Plane)
	{
		int dx = hitresult->point[0]/2.0;
		int dy = hitresult->point[1]/2.0;
		int dz = hitresult->point[2]/2.0;

		if (dx % 2 || dy % 2 || dz % 2 )
		{
			outcolour = Vector3(0.1, 0.1, 0.1);
		}
		else
		{
			outcolour = mat->GetDiffuseColour();
		}
		return outcolour;
	}
	
	////Go through all lights in the scene
	////Note the default scene only has one light source
	if (m_traceflag & TRACE_DIFFUSE_AND_SPEC)
	{
		//TODO: Calculate and apply the lighting of the intersected object using the illumination model covered in the lecture
		while (lit_iter != lights->end())
		{
			Vector3 endpoint = hitresult->point;
			Vector3 lightPosition = (*lit_iter)->GetLightPosition();
			Vector3 l = (lightPosition - endpoint).Normalise();
			Vector3 r = l.Reflect(hitresult->normal);
			Vector3 cam = (endpoint - *campos).Normalise();

			float cosine = l.DotProduct(hitresult->normal);
			float specCosine = r.DotProduct(cam);

			//limit cosine and specCosine to 1 or 0
			if (cosine > 1)
			{
				cosine = 1;
			}
			if (cosine < 0)
			{
				cosine = 0;
			}
			if (specCosine > 1)
			{
				specCosine = 1;
			}
			if (specCosine < 0)
			{
				specCosine = 0;
			}


			//add to outcolor the defuse color * the cosine  
			outcolour = outcolour + (mat->GetDiffuseColour() * cosine);
			
	
			outcolour = outcolour + (mat->GetSpecularColour() * pow(specCosine, mat->GetSpecPower()));

			lit_iter++;
		}
	}

	return outcolour;
}

