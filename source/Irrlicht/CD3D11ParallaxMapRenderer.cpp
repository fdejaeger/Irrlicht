// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "IrrCompileConfig.h"
#ifdef _IRR_COMPILE_WITH_DIRECT3D_11_

#define _IRR_DONT_DO_MEMORY_DEBUGGING_HERE

#include "CD3D11ParallaxMapRenderer.h"
#include "IMaterialRendererServices.h"
#include "IVideoDriver.h"
#include "os.h"
#include "SLight.h"

namespace irr
{
namespace video
{
	const char PARALLAX_MAP_SHADER[] =
		"// adding constant buffer for transform matrices\n"\
		"cbuffer cbPerFrame : register(c0)\n"\
		"{\n"\
		"   float4x4 g_mWorld;\n"\
		"   float4x4 g_mWorldViewProj;\n"\
		"	float4	 g_lightColor1;\n"\
		"	float4	 g_lightColor2;\n"\
		"	float3	 g_eyePosition;\n"\
		"	float3	 g_lightPos1;\n"\
		"	float3	 g_lightPos2;\n"\
		"	float	 g_scaleFactor;\n"\
		"};\n"\
		"\n"\
		"// adding textures and samplers\n"\
		"Texture2D g_tex1 : register(t0);\n"\
		"Texture2D g_tex2 : register(t1);\n"\
		"SamplerState g_sampler1 : register(s0);\n"\
		"SamplerState g_sampler2 : register(s1);\n"\
		"\n"\
		"struct VS_INPUT\n"\
		"{\n"\
		"	float4 pos		: POSITION;\n"\
		"	float3 norm		: NORMAL;\n"\
		"	float4 color	: COLOR;\n"\
		"	float2 tex0		: TEXCOORD0;\n"\
		"	float3 tangent	: TANGENT;\n"\
		"	float3 binormal : BINORMAL;\n"\
		"};\n"\
		"\n"\
		"struct PS_INPUT\n"\
		"{\n"\
		"	float4 pos				: SV_Position;\n"\
		"	float2 colorMapCoord	: TEXTURE0;\n"\
		"	float2 normalMapCoord	: TEXTURE1;\n"\
		"	float3 lightVector1		: TEXTURE2;\n"\
		"	float4 lightColor1		: COLOR0;\n"\
		"	float3 lightVector2		: TEXTURE3;\n"\
		"	float4 lightColor2		: COLOR1;\n"\
		"	float3 eyePos			: TEXTURE4;\n"\
		"};\n"\
		"\n"\
		"PS_INPUT VS(VS_INPUT input)\n"\
		"{\n"\
		"	PS_INPUT output = (PS_INPUT)0;\n"\
		"\n"\
		"	// transform position to clip space\n"\
		"	output.pos = mul( input.pos, g_mWorldViewProj );\n"\
		"\n"\
		"	// vertex - lightpositions\n"\
		"	float4 tempLightVector0 = float4(g_lightPos1, 0.0) - input.pos;\n"\
		"	float4 tempLightVector1 = float4(g_lightPos2, 0.0) - input.pos;\n"\
		"\n"\
		"	// eye vector\n"\
		"	float4 temp = float4(g_eyePosition, 0.f) - input.pos;\n"\
		"\n"\
		"	// transform the light vector 1 with U, V, W\n"\
		"	output.lightVector1.x = dot(input.tangent,  tempLightVector0.xyz);\n"\
		"	output.lightVector1.y = dot(input.binormal, tempLightVector0.xyz);\n"\
		"	output.lightVector1.z = dot(input.norm,   tempLightVector0.xyz);\n"\
		"\n"\
		"	// transform the light vector 2 with U, V, W\n"\
		"	output.lightVector2.x = dot(input.tangent,  tempLightVector1.xyz);\n"\
		"	output.lightVector2.y = dot(input.binormal, tempLightVector1.xyz);\n"\
		"	output.lightVector2.z = dot(input.norm,   tempLightVector1.xyz);\n"\
		"\n"\
		"	// transform the eye vector with U, V, W \n"\
		"	output.eyePos.x = dot(input.tangent,  temp.xyz);\n"\
		"	output.eyePos.y = dot(input.binormal, temp.xyz);\n"\
		"	output.eyePos.z = dot(input.norm,   temp.xyz);\n"\
		"	output.eyePos *= float3(1.0,-1.0, -1.0);\n"\
		"	output.eyePos = normalize(output.eyePos);\n"\
		"\n"\
		"	// calculate attenuation of light 0\n"\
		"	output.lightColor1.w = 0.0;\n"\
		"	output.lightColor1.x = dot(tempLightVector0, tempLightVector0);\n"\
		"	output.lightColor1.x *= g_lightColor1.w;\n"\
		"	output.lightColor1 = rsqrt(output.lightColor1.x);\n"\
		"	output.lightColor1 *= g_lightColor1;\n"\
		"\n"\
		"	// normalize light vector 0\n"\
		"	output.lightVector1 = normalize(output.lightVector1);\n"\
		"\n"\
		"	// calculate attenuation of light 1\n"\
		"	output.lightColor2.w = 0.0;\n"\
		"	output.lightColor2.x = dot(tempLightVector1, tempLightVector1);\n"\
		"	output.lightColor2.x *= g_lightColor2.w;\n"\
		"	output.lightColor2 = rsqrt(output.lightColor2.x);\n"\
		"	output.lightColor2 *= g_lightColor2;\n"\
		"\n"\
		"	// normalize light vector 1\n"\
		"	output.lightVector2 = normalize(output.lightVector2);\n"\
		"\n"\
		"	// move out texture coordinates and original alpha value\n"\
		"	output.colorMapCoord = input.tex0;\n"\
		"	output.normalMapCoord = input.tex0;\n"\
		"	output.lightColor1.a = input.color.a;\n"\
		"\n"\
		"	return output;\n"\
		"}\n"\
		"\n"\
		"// High-definition pixel-shader\n"\
		"float4 PS(PS_INPUT input) : SV_Target\n"\
		"{\n"\
		"	float4 normalMap = g_tex2.Sample( g_sampler2, input.normalMapCoord ).bgra *  2.0 - 1.0;\n"\
		"	normalMap *= g_scaleFactor;\n"\
		"\n"\
		"	// calculate new texture coord: height * eye + oldTexCoord\n"\
		"	float2 offset = input.eyePos.xy * normalMap.w + input.normalMapCoord;\n"\
		"\n"\
		"	// fetch new textures\n"\
		"	float4 colorMap = g_tex1.Sample( g_sampler1, offset ).bgra;\n"\
		"	normalMap = normalize(g_tex2.Sample( g_sampler2, offset ).bgra * 2.0 - 1.0);\n"\
		"\n"\
		"	// calculate color of light 0\n"\
		"	float4 color = clamp(input.lightColor1, 0.0, 1.0) * dot(normalMap.xyz, normalize(input.lightVector1));\n"\
		"\n"\
		"	// calculate color of light 1\n"\
		"	color += clamp(input.lightColor2, 0.0, 1.0) * dot(normalMap.xyz, normalize(input.lightVector2));\n"\
		"\n"\
		"	//luminance * base color\n"\
		"	color *= colorMap;\n"\
		"	color.a = input.lightColor1.a;\n"\
		"\n"\
		"	return color;\n"
		"}\n";

CD3D11ParallaxMapRenderer::CD3D11ParallaxMapRenderer(
	ID3D11Device* device, video::IVideoDriver* driver, CD3D11CallBridge* bridgeCalls,
	s32& outMaterialTypeNr, IMaterialRenderer* baseRenderer)
	: CD3D11MaterialRenderer(device, driver, bridgeCalls, NULL, baseRenderer),
	currentScale(0.0f), cbPerFrameId(-1)
{
#ifdef _DEBUG
	setDebugName("CD3D11ParallaxMapRenderer");
#endif
	
	CallBack = this;

	IMaterialRenderer* renderer = Driver->getMaterialRenderer(EMT_PARALLAX_MAP_SOLID);

	if (renderer)
	{
		CD3D11ParallaxMapRenderer* r = static_cast<CD3D11ParallaxMapRenderer*>(renderer);

		shaders[EST_VERTEX_SHADER] = r->shaders[EST_VERTEX_SHADER];

		if (shaders[EST_VERTEX_SHADER])
			shaders[EST_VERTEX_SHADER]->AddRef();

		shaders[EST_PIXEL_SHADER] = r->shaders[EST_PIXEL_SHADER];

		if (shaders[EST_PIXEL_SHADER])
			shaders[EST_PIXEL_SHADER]->AddRef();

		buffer = r->buffer;
		UserData = r->UserData;

		if(buffer)
			buffer->AddRef();

		cbPerFrameId = r->cbPerFrameId;
	}
	else
	{
		if(driver->queryFeature(EVDF_VERTEX_SHADER_5_0))
		{	
			if(!init(PARALLAX_MAP_SHADER, "VS", EVST_VS_5_0,
				PARALLAX_MAP_SHADER, "PS", EPST_PS_5_0))
				return;
		}
		else
		{
			if(!init(PARALLAX_MAP_SHADER, "VS", EVST_VS_4_1,
				PARALLAX_MAP_SHADER, "PS", EPST_PS_4_1))
				return;
		}	

		cbPerFrameId = getConstantBufferID("cbPerFrame", EST_VERTEX_SHADER);
	}

	// register myself as new material
	outMaterialTypeNr = Driver->addMaterialRenderer(this);
}

CD3D11ParallaxMapRenderer::~CD3D11ParallaxMapRenderer()
{
	if(CallBack == this)
		CallBack = NULL;
}

bool CD3D11ParallaxMapRenderer::OnRender(IMaterialRendererServices* service, E_VERTEX_TYPE vtxtype)
{
	if (vtxtype != video::EVT_TANGENTS)
	{
		os::Printer::log("Error: Parallax map renderer only supports vertices of type EVT_TANGENTS", ELL_ERROR);
		return false;
	}

	return CD3D11MaterialRenderer::OnRender(service, vtxtype);
}

//! Returns the render capability of the material.
s32 CD3D11ParallaxMapRenderer::getRenderCapability() const
{
	if (Driver->queryFeature(video::EVDF_PIXEL_SHADER_4_0) &&
		Driver->queryFeature(video::EVDF_VERTEX_SHADER_4_0))
		return 0;

	return 1;
}

void CD3D11ParallaxMapRenderer::OnSetConstants( IMaterialRendererServices* services, s32 userData )
{
	// Set matrices
	cbPerFrame.g_mWorld = Driver->getTransform(video::ETS_WORLD).getTransposed();

	core::matrix4 minv;
	core::matrix4 mat = Driver->getTransform(video::ETS_PROJECTION);
	mat *=  minv = Driver->getTransform(video::ETS_VIEW);
	mat *= Driver->getTransform(video::ETS_WORLD);
	cbPerFrame.g_mWorldViewProj = mat.getTransposed();

	f32 floats[4] = {0,0,0,1};
	minv.makeInverse();
	minv.multiplyWith1x4Matrix(floats);

	cbPerFrame.g_eyePosition = core::vector3df(floats[0], floats[1], floats[2]);

	// here we've got to fetch the fixed function lights from the
	// driver and set them as constants
	u32 cnt = Driver->getDynamicLightCount();

	SLight light;

	if(cnt >= 1)
		light = Driver->getDynamicLight(0);	
	else
	{
		light.DiffuseColor.set(0,0,0); // make light dark
		light.Radius = 1.0f;
	}

	light.DiffuseColor.a = 1.0f/(light.Radius*light.Radius); // set attenuation

	cbPerFrame.g_lightPos1 = light.Position;
	cbPerFrame.g_lightColor1 = light.DiffuseColor;

	if(cnt >= 2)
		light = Driver->getDynamicLight(1);
	else
	{
		light = SLight();
		light.DiffuseColor.set(0,0,0); // make light dark
		light.Radius = 1.0f;
	}

	light.DiffuseColor.a = 1.0f/(light.Radius*light.Radius); // set attenuation

	cbPerFrame.g_lightPos2 = light.Position;
	cbPerFrame.g_lightColor2 = light.DiffuseColor;

	// set scale factor
	f32 factor = 0.02f; // default value
	if (currentScale != 0)
		factor = currentScale;

	cbPerFrame.g_scaleFactor = factor;

	setConstantBuffer(cbPerFrameId, &cbPerFrame, EST_VERTEX_SHADER);
}

void CD3D11ParallaxMapRenderer::OnSetMaterial( const SMaterial& material )
{
	currentScale = material.MaterialTypeParam;

	CurrentMaterial = material;
}

} // end namespace video
} // end namespace irr

#endif // _IRR_COMPILE_WITH_DIRECT3D_11_