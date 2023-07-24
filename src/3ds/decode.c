#include "3ds.h"

#include <sps.h>

#include <3ds.h>
#include <3ds/types.h>
#include <3ds/services/mvd.h>
#include <3ds/allocator/linear.h>
//#include <citro3d.h>
//#include <gx2/mem.h>

#include <unistd.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

// memory requirement for the maximum supported level (level 42)
//#define H264_MEM_REQUIREMENT (0x2200000 + 0x3ff + 0x480000)
#define H264_MEM_ALIGNMENT 0x40
//#define H264_FRAME_SIZE(w, h) (((w) * (h) * 3) >> 1)
//#define H264_FRAME_PITCH(w) (((w) + 0xff) & ~0xff)
//#define H264_FRAME_HEIGHT(h) (((h) + 0xf) & ~0xf)
//#define H264_MAX_FRAME_SIZE H264_FRAME_SIZE(H264_FRAME_PITCH(2800), 1408)

#define DECODER_BUFFER_SIZE 92*1024

C3D_Tex textures[NUM_BUFFERS];
uint32_t currentTexture;

static MVDSTD_Config config;

static uint8_t* outBuf;
static uint8_t* inBuf;

/*static void createYUVTextures(GX2Texture* yPlane, GX2Texture* uvPlane, uint32_t width, uint32_t height)
{
  memset(yPlane, 0, sizeof(GX2Texture));
  memset(uvPlane, 0, sizeof(GX2Texture));

  // make sure the height is aligned properly for the output
  height = H264_FRAME_HEIGHT(height);

  // create Y texture
  yPlane->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
  yPlane->surface.use = GX2_SURFACE_USE_TEXTURE;
  yPlane->surface.format = GX2_SURFACE_FORMAT_UNORM_R8;
  yPlane->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
  yPlane->surface.depth = 1;
  yPlane->surface.width = width;
  yPlane->surface.height = height;
  yPlane->surface.mipLevels = 1;
  yPlane->viewNumSlices = 1;
  yPlane->viewNumMips = 1;
  yPlane->compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
  GX2CalcSurfaceSizeAndAlignment(&yPlane->surface);
  GX2InitTextureRegs(yPlane);

  // create UV texture
  uvPlane->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
  uvPlane->surface.use = GX2_SURFACE_USE_TEXTURE;
  uvPlane->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8;
  uvPlane->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
  uvPlane->surface.depth = 1;
  uvPlane->surface.width = width / 2;
  uvPlane->surface.height = height / 2;
  uvPlane->surface.mipLevels = 1;
  uvPlane->viewNumSlices = 1;
  uvPlane->viewNumMips = 1;
  uvPlane->compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
  GX2CalcSurfaceSizeAndAlignment(&uvPlane->surface);
  GX2InitTextureRegs(uvPlane);

  yPlane->surface.image = memalign(H264_MEM_ALIGNMENT, H264_FRAME_SIZE(H264_FRAME_PITCH(width), height));
  GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, yPlane->surface.image, H264_FRAME_SIZE(H264_FRAME_PITCH(width), height));

  uvPlane->surface.image = yPlane->surface.image + yPlane->surface.imageSize;
}*/

static int n3ds_decoder_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
  if (videoFormat != VIDEO_FORMAT_H264) {
    printf("Invalid video format\n");
    return -1;
  }

  outBuf = linearMemAlign(0x100000, H264_MEM_ALIGNMENT);
  if (!outBuf) {
    fprintf(stderr, "Failed to allocate outBuf!\n");
    return -1;
  }

  //int res = H264DECCheckMemSegmentation(decoder, H264_MEM_REQUIREMENT);
  //if (res != 0) {
  //  printf("h264_wiiu: Invalid memory segmentation 0x%07X\n", res);
  //  return -1;
  //}
  
  fprintf(stderr, "Initing mvd...\n");
  int res = mvdstdInit(MVDMODE_VIDEOPROCESSING, MVD_INPUT_H264, MVD_OUTPUT_BGR565, MVD_DEFAULT_WORKBUF_SIZE, NULL);
  if (res != 0) {
    printf("mvd_3ds: Error initializing decoder 0x%07X\n", res);
    return -1;
  }

  fprintf(stderr, "Generating mvd config...\n");
  mvdstdGenerateDefaultConfig(&config, height, width, 256, 512, NULL, (uint32_t*)outBuf, (uint32_t*)outBuf);

  //res = H264DECSetParam_FPTR_OUTPUT(decoder, frame_callback);
  //if (res != 0) {
  //  printf("h264_wiiu: Error setting callback 0x%07X\n", res);
  //  return -1;
  //}

  //res = H264DECSetParam_OUTPUT_PER_FRAME(decoder, 1);
  //if (res != 0) {
  //  printf("h264_wiiu: Error setting OUTPUT_PER_FRAME 0x%07X\n", res);
  //  return -1;
  //}

  //res = H264DECOpen(decoder);
  //if (res != 0) {
  //  printf("h264_wiiu: Error opening decoder 0x%07X\n", res);
  //  return -1;
  //}

  //res = H264DECBegin(decoder);
  //if (res != 0) {
  //  printf("h264_wiiu: Error preparing decoder 0x%07X\n", res);
  //  return -1;
  //}

  /*for (int i = 0; i < NUM_BUFFERS; i++) {
    createYUVTextures(&textures[i].yTex, &textures[i].uvTex, width, height);

    res = H264DECCheckMemSegmentation(textures[i].yTex.surface.image, H264_FRAME_SIZE(H264_FRAME_PITCH(width), height));
    if (res != 0) {
      printf("h264_wiiu: Invalid texture memory segmentation 0x%07X\n", res);
      return -1;
    }
  }*/
  currentTexture = 0;

  inBuf = linearMemAlign(DECODER_BUFFER_SIZE + 64, H264_MEM_ALIGNMENT);
  if (!inBuf) {
    fprintf(stderr, "Failed to allocate inBuf!\n");
    return -1;
  }

  return 0;
}

static void n3ds_decoder_cleanup() {
  //H264DECFlush(decoder);
  //H264DECEnd(decoder);
  //H264DECClose(decoder);
  mvdstdExit();

  linearFree(outBuf);
  outBuf = NULL;
  linearFree(inBuf);
  inBuf = NULL;

  /*for (int i = 0; i < NUM_BUFFERS; i++) {
    free(textures[i].yTex.surface.image);
    textures[i].yTex.surface.image = NULL;
  }*/
}

static int n3ds_decoder_submit_decode_unit(PDECODE_UNIT decodeUnit) {
  if (decodeUnit->fullLength > DECODER_BUFFER_SIZE) {
    fprintf(stderr, "Video bitstream buffer too small\n");
    return DR_OK;
  }

  PLENTRY entry = decodeUnit->bufferList;
  int length = 0;
  while (entry != NULL) {
    memcpy(inBuf+length, entry->data, entry->length);
    length += entry->length;
    entry = entry->next;
  }

  MVDSTD_ProcessNALUnitOut tmpout;

  int res = mvdstdProcessVideoFrame(inBuf+4, length, 0, &tmpout);
  if (!MVD_CHECKNALUPROC_SUCCESS(res)) {
    printf("mvd_3ds: Error processing frame 0x%07X\n", res);
    return DR_NEED_IDR;
  }

  C3D_Tex* tex = &textures[currentTexture];
  
  res = mvdstdRenderVideoFrame(&config, true);
  if (res!=MVD_STATUS_OK) {
    printf("mvd_3ds: Error rendering frame 0x%07X\n", res);
    return DR_NEED_IDR;
  }
  
  C3D_TexLoadImage(tex, outBuf, GPU_TEXFACE_2D, 0);
  
  nextFrame++;

  add_frame(tex);

  currentTexture++;

  if (currentTexture >= NUM_BUFFERS) {
    currentTexture = 0;
  }

  return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds = {
  .setup = n3ds_decoder_setup,
  .cleanup = n3ds_decoder_cleanup,
  .submitDecodeUnit = n3ds_decoder_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT,
};
