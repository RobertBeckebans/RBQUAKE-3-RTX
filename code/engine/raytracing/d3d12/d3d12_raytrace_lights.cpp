// d3d12_raytrace_lights.cpp
//

#include "d3d12_local.h"
#include "../math/vectormath.h"

#define MAX_DRAW_LIGHTS	 120
#define MAX_WORLD_LIGHTS 512

struct glLight_t
{
	vec4_t				origin_radius;
	vec4_t				light_color;
	vec4_t				light_color2;
	vec3_t				absmin;
	vec3_t				absmax;
	vec4_t				light_clamp;
	lightDistanceType_t attenuation;

	// entity_t* ent;
	int					leafnums[16];
	int					lightStyle;

	int					num_leafs;
	int					distance;

	bool				isAreaLight;
};

struct sceneLightInfo_t
{
	vec4_t origin_radius;
	vec4_t light_color;
	vec4_t light_clamp;
	vec4_t light_color2;
};

glLight_t		  worldLights[MAX_WORLD_LIGHTS];
glLight_t		  worldLightsSorted[MAX_WORLD_LIGHTS];
int				  numWorldLights = 0;

sceneLightInfo_t* sceneLights = NULL;
tr_buffer*		  sceneLightInfoBuffer;

int				  numStaticWorldLights = 0;

/*
===============
GL_ClearLights
===============
*/
void			  GL_SetNumMapLights()
{
	numStaticWorldLights = numWorldLights;
}

/*
===============
GL_ClearLights
===============
*/
void GL_ClearLights( void )
{
	memset( &worldLights[0], 0, sizeof( worldLights ) );
	numWorldLights		 = 0;
	numStaticWorldLights = 0;

	if( sceneLightInfoBuffer != NULL )
	{
		tr_destroy_buffer( renderer, sceneLightInfoBuffer );
		sceneLightInfoBuffer = NULL;
	}
}

/*
===============
GL_FindTouchedLeafs
===============
*/
void GL_FindTouchedLeafs( glLight_t* ent, mnode_t* node )
{
	// mplane_t* splitplane;
	// mleaf_t* leaf;
	// int			sides;
	// int			leafnum;
	//
	// if (node->contents == CONTENTS_SOLID)
	//	return;
	//
	//// add an efrag if the node is a leaf
	//
	// if (node->contents < 0)
	//{
	//	if (ent->num_leafs == MAX_ENT_LEAFS)
	//		return;
	//
	//	leaf = (mleaf_t*)node;
	//	leafnum = leaf - loadmodel->leafs - 1;
	//
	//	ent->leafnums[ent->num_leafs] = leafnum;
	//	ent->num_leafs++;
	//	return;
	//}
	//
	//// NODE_MIXED
	//
	// splitplane = node->plane;
	// sides = BOX_ON_PLANE_SIDE(ent->absmin, ent->absmax, splitplane);
	//
	//// recurse down the contacted sides
	// if (sides & 1)
	//	GL_FindTouchedLeafs(ent, node->children[0]);
	//
	// if (sides & 2)
	//	GL_FindTouchedLeafs(ent, node->children[1]);
}

void GL_RegisterWorldLight( refEntity_t* ent, float x, float y, float z, float radius, int lightStyle, float r, float g, float b, lightDistanceType_t attenuation )
{
	glLight_t light = {};

	if( numWorldLights >= MAX_WORLD_LIGHTS )
	{
		// Com_Printf("MAX_WORLD_LIGHTS!\n");
		return;
	}

	light.origin_radius[0] = x;
	light.origin_radius[1] = y;
	light.origin_radius[2] = z;
	light.origin_radius[3] = radius;

	light.light_color[0] = r;
	light.light_color[1] = g;
	light.light_color[2] = b;

	light.absmin[0] = x;
	light.absmin[1] = y;
	light.absmin[2] = z;

	light.absmax[0] = x;
	light.absmax[1] = y;
	light.absmax[2] = z;

	light.lightStyle  = lightStyle;
	light.attenuation = attenuation;

	// light.ent = ent;
	light.num_leafs = 0;
	// GL_FindTouchedLeafs(&light, loadmodel->nodes);

	worldLights[numWorldLights++] = light;
}

void GL_RegisterWorldAreaLight( vec3_t normal, vec3_t mins, vec3_t maxs, int lightStyle, float radius, float r, float g, float b )
{
	glLight_t light = {};
	vec3_t	  origin;
	vec3_t	  light_clamp;

	VectorAdd( maxs, mins, origin );
	VectorSubtract( maxs, mins, light_clamp );

	light.light_clamp[0] = light_clamp[0];
	light.light_clamp[1] = light_clamp[1];
	light.light_clamp[2] = light_clamp[2];

	light.origin_radius[0] = origin[0] * 0.5f;
	light.origin_radius[1] = origin[1] * 0.5f;
	light.origin_radius[2] = origin[2] * 0.5f;
	light.origin_radius[3] = radius; // area light

	light.light_color[0] = normal[0];
	light.light_color[1] = normal[1];
	light.light_color[2] = normal[2];

	light.light_color2[0] = r;
	light.light_color2[1] = g;
	light.light_color2[2] = b;

	light.absmin[0] = mins[0];
	light.absmin[1] = mins[1];
	light.absmin[2] = mins[2];

	light.absmax[0] = maxs[0];
	light.absmax[1] = maxs[1];
	light.absmax[2] = maxs[2];

	light.lightStyle = lightStyle;

	light.isAreaLight = true;

	// Quake sub divides geometry(lovely) so to hack around that don't add any area lights that are near already registered area lights!
	for( int i = 0; i < numWorldLights; i++ )
	{
		float dist = Distance( light.origin_radius, worldLights[i].origin_radius );
		if( dist < 75 )
			return;
	}

	if( numWorldLights >= MAX_WORLD_LIGHTS )
	{
		// Com_Printf("MAX_WORLD_LIGHTS!\n");
		return;
	}

	light.num_leafs				  = -1; // arealight
	worldLights[numWorldLights++] = light;
}

void GL_InitLightInfoBuffer( D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr )
{
	tr_create_uniform_buffer( renderer, sizeof( sceneLightInfo_t ) * MAX_DRAW_LIGHTS, true, &sceneLightInfoBuffer );
	sceneLights = ( sceneLightInfo_t* )sceneLightInfoBuffer->cpu_mapped_address;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	uint32_t						bufferSize = ROUND_UP( sizeof( sceneLightInfo_t ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );

	srvDesc.Shader4ComponentMapping	   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format					   = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension			   = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement		   = 0;
	srvDesc.Buffer.NumElements		   = MAX_DRAW_LIGHTS;
	srvDesc.Buffer.StructureByteStride = sizeof( sceneLightInfo_t );
	srvDesc.Buffer.Flags			   = D3D12_BUFFER_SRV_FLAG_NONE;
	// Write the per-instance properties buffer view in the heap
	m_device->CreateShaderResourceView( sceneLightInfoBuffer->dx_resource, &srvDesc, srvPtr );
}

int lightSort( const void* a, const void* b )
{
	glLight_t light1 = *( ( glLight_t* )a );
	glLight_t light2 = *( ( glLight_t* )b );

	return light1.distance - light2.distance;
}

void GL_BuildLightList( float x, float y, float z )
{
	int numVisLights = 0;

	if( sceneLights == NULL )
		return;

	memset( sceneLights, 0, sizeof( sceneLightInfo_t ) * MAX_DRAW_LIGHTS );

	memcpy( worldLightsSorted, worldLights, sizeof( worldLights ) );

	for( int i = 0; i < numWorldLights; i++ )
	{
		glLight_t* ent	   = &worldLightsSorted[i];
		vec3_t	   viewpos = { x, y, z };

		ent->distance = Distance( ent->origin_radius, viewpos );
	}

	qsort( worldLightsSorted, numWorldLights, sizeof( glLight_t ), lightSort );

	for( int i = 0; i < numWorldLights; i++ )
	{
		if( numVisLights >= MAX_DRAW_LIGHTS )
		{
			// Com_Printf("MAX_DRAW_LIGHTS!\n");
			break;
		}

		glLight_t* ent = &worldLightsSorted[i];

		// if (!ent->isAreaLight)
		//{
		//	if (!ri.PF_inPVS(r_newrefdef.vieworg, ent->origin_radius))
		//		continue;
		// }

		sceneLights[numVisLights].origin_radius[0] = ent->origin_radius[0];
		sceneLights[numVisLights].origin_radius[1] = ent->origin_radius[1];
		sceneLights[numVisLights].origin_radius[2] = ent->origin_radius[2];

		if( !ent->isAreaLight )
		{
			sceneLights[numVisLights].origin_radius[3] = ent->origin_radius[3];
		}
		else
		{
			sceneLights[numVisLights].origin_radius[3] = -ent->origin_radius[3];
		}

		// if (ent->lightStyle) {
		//	sceneLights[numVisLights].light_color[0] = ent->light_color[0] * r_newrefdef.lightstyles[ent->lightStyle].rgb[0];
		//	sceneLights[numVisLights].light_color[1] = ent->light_color[1] * r_newrefdef.lightstyles[ent->lightStyle].rgb[1];
		//	sceneLights[numVisLights].light_color[2] = ent->light_color[2] * r_newrefdef.lightstyles[ent->lightStyle].rgb[2];
		// }
		// else {
		sceneLights[numVisLights].light_color[0] = ent->light_color[0];
		sceneLights[numVisLights].light_color[1] = ent->light_color[1];
		sceneLights[numVisLights].light_color[2] = ent->light_color[2];
		sceneLights[numVisLights].light_color[3] = ent->attenuation;
		//}

		sceneLights[numVisLights].light_clamp[0] = ent->light_clamp[0];
		sceneLights[numVisLights].light_clamp[1] = ent->light_clamp[1];
		sceneLights[numVisLights].light_clamp[2] = ent->light_clamp[2];

		sceneLights[numVisLights].light_color2[0] = ent->light_color2[0];
		sceneLights[numVisLights].light_color2[1] = ent->light_color2[1];
		sceneLights[numVisLights].light_color2[2] = ent->light_color2[2];

		numVisLights++;
	}

	numWorldLights = numStaticWorldLights;
}