#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include "vshader_shbin.h"

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct {
	float x, y, z;
	unsigned int color1;
	unsigned int color2;
} vertex;

static const vertex vertex_list[] =
{
	{ 200.0f, 200.0f, 0.5f, 0xFFFF00FF, 0xFF0000FF },
	{ 100.0f, 40.0f, 0.5f, 0xFFFF00FF, 0xFF0000FF },
	{ 300.0f, 40.0f, 0.5f, 0xFFFF00FF, 0xFF0000FF },
};

#define vertex_list_count (sizeof(vertex_list)/sizeof(vertex_list[0]))

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static C3D_Mtx projection;
C3D_BufInfo bufInfo1;
C3D_BufInfo bufInfo2;
C3D_BufInfo* currentBuffer = &bufInfo1;

static void* vbo_data;

static void sceneInit(void)
{
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	// Get the location of the uniforms
	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

	// Configure attributes for use with the vertex shader
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_UNSIGNED_BYTE, 4); // v1=color

	// Set the fixed attribute (color) to solid white
	//C3D_FixedAttribSet(1, 1.0, 1.0, 1.0, 1.0);

	// Compute the projection matrix
	Mtx_OrthoTilt(&projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0);

	// Create the VBO (vertex buffer object)
	vbo_data = linearAlloc(sizeof(vertex_list));
	memcpy(vbo_data, vertex_list, sizeof(vertex_list));

	// Configure buffers
	BufInfo_Init(&bufInfo1);
	BufInfo_Add(&bufInfo1, vbo_data, sizeof(vertex), 3, 0xC10);
	
	BufInfo_Init(&bufInfo2);
	BufInfo_Add(&bufInfo2, vbo_data, sizeof(vertex), 3, 0x1C0);

	// Configure the first fragment shading substage to just pass through the vertex color
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

static void sceneRender(void)
{
	// bind a buffer
	C3D_SetBufInfo(currentBuffer);
	
	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

	// Draw the VBO
	C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_list_count);
}

static void sceneExit(void)
{
	// Free the VBO
	linearFree(vbo_data);

	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
}

int main()
{
	// Initialize graphics
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	// Initialize the renderbuffer
	static C3D_RenderBuf rb;
	C3D_RenderBufInit(&rb, 240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	rb.clearColor = CLEAR_COLOR;
	C3D_RenderBufClear(&rb);
	C3D_RenderBufBind(&rb);

	// Initialize the scene
	sceneInit();

	// Main loop
	while (aptMainLoop())
	{
		C3D_VideoSync();
		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu
		if (kDown & KEY_A)
			currentBuffer = &bufInfo1;
		if (kDown & KEY_B)
			currentBuffer = &bufInfo2;
		
		// Render the scene
		sceneRender();
		C3D_Flush();
		C3D_RenderBufTransfer(&rb, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), DISPLAY_TRANSFER_FLAGS);
		C3D_RenderBufClear(&rb);
	}

	// Deinitialize the scene
	sceneExit();

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();
	return 0;
}
