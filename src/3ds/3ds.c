#include "3ds.h"

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>


//#include <whb/proc.h>
//#include <3ds/gfx.h>
#include <3ds/synchronization.h>
//#include <gx2/mem.h>
//#include <gx2/draw.h>
//#include <gx2/registers.h>

//#include "shaders/display.h"


#define ATTRIB_SIZE (8 * 2 * sizeof(float))
#define ATTRIB_STRIDE (4 * sizeof(float))

static GX2Sampler screenSamp;
//static WHBGfxShaderGroup shaderGroup;

static float* tvAttribs;
static float* drcAttribs;

static float tvScreenSize[2];
static float drcScreenSize[2];

uint32_t currentFrame;
uint32_t nextFrame;

static RecursiveLock queueMutex;
static yuv_texture_t* queueMessages[MAX_QUEUEMESSAGES];
static uint32_t queueWriteIndex;
static uint32_t queueReadIndex;

void ds_stream_init(uint32_t width, uint32_t height)
{
  currentFrame = nextFrame = 0;

  RecursiveLock_Init(&queueMutex);
  queueReadIndex = queueWriteIndex = 0;

  /*if (!WHBGfxLoadGFDShaderGroup(&shaderGroup, 0, display_gsh)) {
    printf("Cannot load shader\n");
  }*/

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
  GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, drcAttribs, ATTRIB_SIZE);
}

void ds_stream_draw(void)
{
  yuv_texture_t* tex = get_frame();
  if (tex) {
    if (++currentFrame <= nextFrame - NUM_BUFFERS) {
      // display thread is behind decoder, skip frame
    }
    else {
      u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
      memcpy(fb, buffer?, 400*240*2); // i need to somehow obtain a buffer address and a buffer size
    }
  }
}

void ds_stream_fini(void)
{
  free(tvAttribsyuv_texture_t);
  free(drcAttribs);

  WHBGfxFreeShaderGroup(&shaderGroup);
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
  yuv_texture_t* message = queueMessages[i];

  RecursiveLock_Unlock(&queueMutex);
  return message;
}

void add_frame(yuv_texture_t* msg)
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

void ds_setup_renderstate(void)
{
  WHBGfxBeginRenderTV();
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
  GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
}
