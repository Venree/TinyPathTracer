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

#include "PathTracer.h"
#include "Ray.h"
#include "Scene.h"
#include "Camera.h"
#include "perlin.h"

#define M_PI 3.14159265358979323846

PathTracer::PathTracer()
{
	m_buffHeight = m_buffWidth = 0.0;
	m_renderCount = 0;
	SetTraceLevel(0);
	m_traceflag = (TraceFlags)(TRACE_PATH);
}

PathTracer::PathTracer(int Width, int Height)
{
	m_buffWidth = Width;
	m_buffHeight = Height;
	m_renderCount = 0;
	SetTraceLevel(0);

	m_framebuffer = new Framebuffer(Width, Height);

	//default set default trace flag, i.e. no lighting, non-recursive
	m_traceflag = (TraceFlags)(TRACE_PATH);
}

PathTracer::~PathTracer()
{
	delete m_framebuffer;
}

double RND()
{
	return (double)rand() / (double)RAND_MAX;
}

void PathTracer::DoPathTrace(Scene* pScene)
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

	int samps = 100; //# of samples

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
		for (int i = 0; i < m_buffHeight; i += 1) {
			for (int j = 0; j < m_buffWidth; j += 1) {
				
				for (int f = 0; f < samps; f++)
				{
					Vector3 pixel;
					pixel[0] = start[0] + (i + 0.5) * camUpVector[0] * pixelDY
						+ (j + 0.5) * camRightVector[0] * pixelDX;
					pixel[1] = start[1] + (i + 0.5) * camUpVector[1] * pixelDY
						+ (j + 0.5) * camRightVector[1] * pixelDX;
					pixel[2] = start[2] + (i + 0.5) * camUpVector[2] * pixelDY
						+ (j + 0.5) * camRightVector[2] * pixelDX;

					Ray viewray;
					
					viewray.SetRay(camPosition, (pixel - camPosition).Normalise());

					double u = (double)j / (double)m_buffWidth;
					double v = (double)i / (double)m_buffHeight;

					scenebg = pScene->GetBackgroundColour();

					//trace the scene using the view ray
					//default colour is the background colour, unless something is hit along the way
					colour = colour + this->Radiance(pScene, viewray, 0);
					
				}
				colour = colour * (1./samps);
				m_framebuffer->WriteRGBToFramebuffer(colour, j, i);
			}
			//printf("\n%d", i); //debug counter when number reaches 500 render is complete
		}

		fprintf(stdout, "Done!!!\n");
		m_renderCount++;
	}
}

Colour PathTracer::Radiance(Scene* pScene, Ray& ray,  int tracelevel, bool shadowray)
{
	RayHitResult result;

	Colour outcolour; //= pScene->GetBackgroundColour();
	result = pScene->IntersectByRay(ray);

	if (!result.data) return Vector3(); // if miss return black

	Primitive* prim = (Primitive*)result.data;
	Material* mat = prim->GetMaterial();

	if (result.data) //the ray has hit something
	{

		Colour diffuse = mat->GetDiffuseColour(); //a b c = rgb of mat diffuse colour
		Colour emittance = mat->GetEmissiveColour();

		Vector3 n = result.normal;

		//double p = a > b && a > c ? a : b > c ? b : c;
		double p = diffuse[0] > diffuse[1] && diffuse[0] > diffuse[2] ? diffuse[0] : diffuse[1] > diffuse[2] ? diffuse[1] : diffuse[2];
		
		if (++tracelevel > 5)
		{
			//Dice roll
			if (RND() < p)
			{
				diffuse = diffuse * (1 / p);
			}
			else
			{
				return emittance; 
			}
		}

		double r1 = 2 * M_PI*RND(), r2 = RND(), r2s = sqrt(r2);

		Vector3 u = (fabs(n[0]) > 0.1 ? Vector3(0, 1, 0) : Vector3(1, 0, 0)).CrossProduct(n).Normalise();
		Vector3 v = n.CrossProduct(u);
		Vector3 d = (u*cos(r1)*r2s + v *sin(r1)*r2s + n*sqrt(1 - r2)).Normalise();
		Ray difRay, refRay;
		difRay.SetRay(result.point + d *0.0001f, d);
		
		if (((Primitive*)result.data)->m_primtype == Primitive::PRIMTYPE_Box || ((Primitive*)result.data)->m_primtype == Primitive::PRIMTYPE_Sphere)
		{
			Vector3 reflVec = (ray.GetRay().Reflect(n)).Normalise();
			refRay.SetRay(result.point + reflVec * 0.001f, reflVec);
		}

		outcolour = (((Primitive*)result.data)->m_primtype == Primitive::PRIMTYPE_Plane || !(RND() < p)) ? 
			emittance + diffuse * (Radiance(pScene, difRay, tracelevel)) :
			emittance + diffuse * (Radiance(pScene, refRay, tracelevel));
		return outcolour;

	}
	
}