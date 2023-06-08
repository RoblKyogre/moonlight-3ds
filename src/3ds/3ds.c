#include "3ds.h"

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>


//#include <whb/proc.h>
#include <3ds/synchronization.h>

#include <3ds/gfx.h>
#include <3ds/types.h>
#include <3ds/gpu/gpu.h>
#include <3ds/gpu/shaderProgram.h>
#include <3ds/util/decompress.h>

#include <c3d/base.h>
#include <c3d/texture.h>
#include <c2d/base.h>

//#include <gx2/mem.h>
//#include <gx2/draw.h>
//#include <gx2/registers.h>

//#include "shaders/display.h"


#define ATTRIB_SIZE (8 * 2 * sizeof(float))
#define ATTRIB_STRIDE (4 * sizeof(float))

//static GX2Sampler screenSamp;
//static WHBGfxShaderGroup shaderGroup;

static C3D_RenderTarget* topScreen;
static C2D_Image img;

static C2D_DrawParams screenParams;

static float* tvAttribs;
static float* drcAttribs;

static float tvScreenSize[2];
static float drcScreenSize[2];

uint32_t currentFrame;
uint32_t nextFrame;

static RecursiveLock queueMutex;
static C3D_Tex* queueMessages[MAX_QUEUEMESSAGES];
static uint32_t queueWriteIndex;
static uint32_t queueReadIndex;

const Tex3DS_SubTexture subtex = {
  512, 256,
  0.0f, 1.0f, 1.0f, 0.0f
};

void n3ds_stream_init(uint32_t width, uint32_t height)
{
  currentFrame = nextFrame = 0;

  RecursiveLock_Init(&queueMutex);
  queueReadIndex = queueWriteIndex = 0;
  
  gfxInitDefault();
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
  
  topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

  img.subtex = &subtex;

  screenParams.pos.x = 0.0f;
  screenParams.pos.y = 0.0f;
  screenParams.pos.w = 400;
  screenParams.pos.h = 240;
  
  screenParams.depth = 0.0f;
  screenParams.angle = 0.0f;

  /*if (!WHBGfxLoadGFDShaderGroup(&shaderGroup, 0, display_gsh)) {
    printf("Cannot load shader\n");
  }

  WHBGfxInitShaderAttribute(&shaderGroup, "in_pos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
  WHBGfxInitShaderAttribute(&shaderGroup, "in_texCoord", 0, 8, GX2_ATTRIB_FORMAT_FLOAT_32_32);

  if (!WHBGfxInitFetchShader(&shaderGroup)) {
    printf("cannot init shader\n");
  }

  GX2InitSampler(&screenSamp, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_POINT);

  GX2ColorBuffer* cb = WHBGfxGetTVColourBuffer();
  tvScreenSize[0] = 1.0f / (float) cb->surface.width;
  tvScreenSize[1] = 1.0f / (float) cb->surface.height;

  tvAttribs = memalign(GX2_VERTEX_BUFFER_ALIGNMENT, ATTRIB_SIZE);
  int i = 0;

  tvAttribs[i++] = 0.0f;                      tvAttribs[i++] = 0.0f;
  tvAttribs[i++] = 0.0f;                      tvAttribs[i++] = 0.0f;

  tvAttribs[i++] = (float) cb->surface.width; tvAttribs[i++] = 0.0f;
  tvAttribs[i++] = 1.0f;                      tvAttribs[i++] = 0.0f;

  tvAttribs[i++] = (float) cb->surface.width; tvAttribs[i++] = (float) cb->surface.height;
  tvAttribs[i++] = 1.0f;                      tvAttribs[i++] = 1.0f;

  tvAttribs[i++] = 0.0f;                      tvAttribs[i++] = (float) cb->surface.height;
  tvAttribs[i++] = 0.0f;                      tvAttribs[i++] = 1.0f;
  GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, tvAttribs, ATTRIB_SIZE);

  cb = WHBGfxGetDRCColourBuffer();
  drcScreenSize[0] = 1.0f / (float) cb->surface.width;
  drcScreenSize[1] = 1.0f / (float) cb->surface.height;

  drcAttribs = memalign(GX2_VERTEX_BUFFER_ALIGNMENT, ATTRIB_SIZE);
  i = 0;

  drcAttribs[i++] = 0.0f;                      drcAttribs[i++] = 0.0f;
  drcAttribs[i++] = 0.0f;                      drcAttribs[i++] = 0.0f;

  drcAttribs[i++] = (float) cb->surface.width; drcAttribs[i++] = 0.0f;
  drcAttribs[i++] = 1.0f;                      drcAttribs[i++] = 0.0f;

  drcAttribs[i++] = (float) cb->surface.width; drcAttribs[i++] = (float) cb->surface.height;
  drcAttribs[i++] = 1.0f;                      drcAttribs[i++] = 1.0f;

  drcAttribs[i++] = 0.0f;                      drcAttribs[i++] = (float) cb->surface.height;
  drcAttribs[i++] = 0.0f;                      drcAttribs[i++] = 1.0f;
  GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, drcAttribs, ATTRIB_SIZE);*/
}

void n3ds_stream_draw(void)
{
  C3D_Tex* tex = get_frame();
  if (tex) {
    if (++currentFrame <= nextFrame - NUM_BUFFERS) {
      // display thread is behind decoder, skip frame
    }
    else {
      img.tex = tex;
      C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
      C2D_TargetClear(topScreen, C2D_Color32f(0,0,0,1));
      C2D_SceneBegin(topScreen);
      C2D_DrawImage(img,&screenParams,NULL);
      C3D_FrameEnd(0);
    }
  }
}

void n3ds_stream_fini(void)
{
  C2D_Fini();
  C3D_Fini();
}

void* get_frame(void)
{
  RecursiveLock_Lock(&queueMutex);

  uint32_t elements_in = queueWriteIndex - queueReadIndex;
  if(elements_in == 0) {
    RecursiveLock_Unlock(&queueMutex);
    return NULL; // framequeue is empty
  }

  uint32_t i = (queueReadIndex)++ & (MAX_QUEUEMESSAGES - 1);
  C3D_Tex* message = queueMessages[i];

  RecursiveLock_Unlock(&queueMutex);
  return message;
}

void add_frame(C3D_Tex* msg)
{
  RecursiveLock_Lock(&queueMutex);

  uint32_t elements_in = queueWriteIndex - queueReadIndex;
  if (elements_in == MAX_QUEUEMESSAGES) {
    RecursiveLock_Unlock(&queueMutex);
    return; // framequeue is full
  }

  uint32_t i = (queueWriteIndex)++ & (MAX_QUEUEMESSAGES - 1);
  queueMessages[i] = msg;

  RecursiveLock_Unlock(&queueMutex);
}

void n3ds_setup_renderstate(void)
{
  C2D_Prepare();

  /*WHBGfxBeginRenderTV();
  GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
  GX2SetBlendControl(GX2_RENDER_TARGET_0,
    GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD,
    TRUE,
    GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD
  );
  GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
  WHBGfxBeginRenderDRC();
  GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
  GX2SetBlendControl(GX2_RENDER_TARGET_0,
    GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD,
    TRUE,
    GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD
  );
  GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);*/
}
