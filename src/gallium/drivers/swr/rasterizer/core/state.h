/****************************************************************************
* Copyright (C) 2014-2015 Intel Corporation.   All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*
* @file state.h
*
* @brief Definitions for API state.
*
******************************************************************************/
#pragma once

#include "common/formats.h"
#include "common/simdintrin.h"

// clear flags
#define SWR_CLEAR_NONE        0
#define SWR_CLEAR_COLOR      (1 << 0)
#define SWR_CLEAR_DEPTH      (1 << 1)
#define SWR_CLEAR_STENCIL    (1 << 2)

enum DRIVER_TYPE
{
    DX,
    GL
};

//////////////////////////////////////////////////////////////////////////
/// PRIMITIVE_TOPOLOGY.
//////////////////////////////////////////////////////////////////////////
enum PRIMITIVE_TOPOLOGY
{
    TOP_UNKNOWN = 0x0,
    TOP_POINT_LIST = 0x1,
    TOP_LINE_LIST = 0x2,
    TOP_LINE_STRIP = 0x3,
    TOP_TRIANGLE_LIST = 0x4,
    TOP_TRIANGLE_STRIP = 0x5,
    TOP_TRIANGLE_FAN = 0x6,
    TOP_QUAD_LIST = 0x7,
    TOP_QUAD_STRIP = 0x8,
    TOP_LINE_LIST_ADJ = 0x9,
    TOP_LISTSTRIP_ADJ = 0xA,
    TOP_TRI_LIST_ADJ = 0xB,
    TOP_TRI_STRIP_ADJ = 0xC,
    TOP_TRI_STRIP_REVERSE = 0xD,
    TOP_POLYGON = 0xE,
    TOP_RECT_LIST = 0xF,
    TOP_LINE_LOOP = 0x10,
    TOP_POINT_LIST_BF = 0x11,
    TOP_LINE_STRIP_CONT = 0x12,
    TOP_LINE_STRIP_BF = 0x13,
    TOP_LINE_STRIP_CONT_BF = 0x14,
    TOP_TRIANGLE_FAN_NOSTIPPLE = 0x16,
    TOP_TRIANGLE_DISC = 0x17,   /// @todo What is this??

    TOP_PATCHLIST_BASE = 0x1F,  // Invalid topology, used to calculate num verts for a patchlist.
    TOP_PATCHLIST_1 = 0x20,     // List of 1-vertex patches
    TOP_PATCHLIST_2 = 0x21,
    TOP_PATCHLIST_3 = 0x22,
    TOP_PATCHLIST_4 = 0x23,
    TOP_PATCHLIST_5 = 0x24,
    TOP_PATCHLIST_6 = 0x25,
    TOP_PATCHLIST_7 = 0x26,
    TOP_PATCHLIST_8 = 0x27,
    TOP_PATCHLIST_9 = 0x28,
    TOP_PATCHLIST_10 = 0x29,
    TOP_PATCHLIST_11 = 0x2A,
    TOP_PATCHLIST_12 = 0x2B,
    TOP_PATCHLIST_13 = 0x2C,
    TOP_PATCHLIST_14 = 0x2D,
    TOP_PATCHLIST_15 = 0x2E,
    TOP_PATCHLIST_16 = 0x2F,
    TOP_PATCHLIST_17 = 0x30,
    TOP_PATCHLIST_18 = 0x31,
    TOP_PATCHLIST_19 = 0x32,
    TOP_PATCHLIST_20 = 0x33,
    TOP_PATCHLIST_21 = 0x34,
    TOP_PATCHLIST_22 = 0x35,
    TOP_PATCHLIST_23 = 0x36,
    TOP_PATCHLIST_24 = 0x37,
    TOP_PATCHLIST_25 = 0x38,
    TOP_PATCHLIST_26 = 0x39,
    TOP_PATCHLIST_27 = 0x3A,
    TOP_PATCHLIST_28 = 0x3B,
    TOP_PATCHLIST_29 = 0x3C,
    TOP_PATCHLIST_30 = 0x3D,
    TOP_PATCHLIST_31 = 0x3E,
    TOP_PATCHLIST_32 = 0x3F,   // List of 32-vertex patches
};

//////////////////////////////////////////////////////////////////////////
/// SWR_SHADER_TYPE
//////////////////////////////////////////////////////////////////////////
enum SWR_SHADER_TYPE
{
    SHADER_VERTEX,
    SHADER_GEOMETRY,
    SHADER_DOMAIN,
    SHADER_HULL,
    SHADER_PIXEL,
    SHADER_COMPUTE,

    NUM_SHADER_TYPES,
};

//////////////////////////////////////////////////////////////////////////
/// SWR_RENDERTARGET_ATTACHMENT
/// @todo Its not clear what an "attachment" means. Its not common term.
//////////////////////////////////////////////////////////////////////////
enum SWR_RENDERTARGET_ATTACHMENT
{
    SWR_ATTACHMENT_COLOR0,
    SWR_ATTACHMENT_COLOR1,
    SWR_ATTACHMENT_COLOR2,
    SWR_ATTACHMENT_COLOR3,
    SWR_ATTACHMENT_COLOR4,
    SWR_ATTACHMENT_COLOR5,
    SWR_ATTACHMENT_COLOR6,
    SWR_ATTACHMENT_COLOR7,
    SWR_ATTACHMENT_DEPTH,
    SWR_ATTACHMENT_STENCIL,

    SWR_NUM_ATTACHMENTS
};

#define SWR_NUM_RENDERTARGETS 8

#define SWR_ATTACHMENT_COLOR0_BIT 0x001
#define SWR_ATTACHMENT_COLOR1_BIT 0x002
#define SWR_ATTACHMENT_COLOR2_BIT 0x004
#define SWR_ATTACHMENT_COLOR3_BIT 0x008
#define SWR_ATTACHMENT_COLOR4_BIT 0x010
#define SWR_ATTACHMENT_COLOR5_BIT 0x020
#define SWR_ATTACHMENT_COLOR6_BIT 0x040
#define SWR_ATTACHMENT_COLOR7_BIT 0x080
#define SWR_ATTACHMENT_DEPTH_BIT 0x100
#define SWR_ATTACHMENT_STENCIL_BIT 0x200
#define SWR_ATTACHMENT_MASK_ALL 0x3ff
#define SWR_ATTACHMENT_MASK_COLOR 0x0ff


//////////////////////////////////////////////////////////////////////////
/// @brief SWR Inner Tessellation factor ID
/// See above GetTessFactorOutputPosition code for documentation
enum SWR_INNER_TESSFACTOR_ID
{
    SWR_QUAD_U_TRI_INSIDE,
    SWR_QUAD_V_INSIDE,

    SWR_NUM_INNER_TESS_FACTORS,
};

//////////////////////////////////////////////////////////////////////////
/// @brief SWR Outer Tessellation factor ID
/// See above GetTessFactorOutputPosition code for documentation
enum SWR_OUTER_TESSFACTOR_ID
{
    SWR_QUAD_U_EQ0_TRI_U_LINE_DETAIL,
    SWR_QUAD_V_EQ0_TRI_V_LINE_DENSITY,
    SWR_QUAD_U_EQ1_TRI_W,
    SWR_QUAD_V_EQ1,

    SWR_NUM_OUTER_TESS_FACTORS,
};


/////////////////////////////////////////////////////////////////////////
/// simdvertex
/// @brief Defines a vertex element that holds all the data for SIMD vertices.
///        Contains position in clip space, hardcoded to attribute 0,
///        space for up to 32 attributes, as well as any SGV values generated
///        by the pipeline
/////////////////////////////////////////////////////////////////////////
#define VERTEX_POSITION_SLOT 0
#define VERTEX_ATTRIB_START_SLOT 1
#define VERTEX_ATTRIB_END_SLOT 32
#define VERTEX_RTAI_SLOT 33         // GS writes RenderTargetArrayIndex here
#define VERTEX_PRIMID_SLOT 34       // GS writes PrimId here
#define VERTEX_CLIPCULL_DIST_LO_SLOT 35 // VS writes lower 4 clip/cull dist
#define VERTEX_CLIPCULL_DIST_HI_SLOT 36 // VS writes upper 4 clip/cull dist
#define VERTEX_POINT_SIZE_SLOT 37       // VS writes point size here
static_assert(VERTEX_POINT_SIZE_SLOT < KNOB_NUM_ATTRIBUTES, "Mismatched attribute slot size");

// SoAoSoA
struct simdvertex
{
    simdvector    attrib[KNOB_NUM_ATTRIBUTES];
};

//////////////////////////////////////////////////////////////////////////
/// SWR_VS_CONTEXT
/// @brief Input to vertex shader
/////////////////////////////////////////////////////////////////////////
struct SWR_VS_CONTEXT
{
    simdvertex* pVin;           // IN: SIMD input vertex data store
    simdvertex* pVout;          // OUT: SIMD output vertex data store

    uint32_t InstanceID;    // IN: Instance ID, constant across all verts of the SIMD
    simdscalari VertexID;   // IN: Vertex ID
    simdscalari mask;       // IN: Active mask for shader
};

/////////////////////////////////////////////////////////////////////////
/// ScalarCPoint
/// @brief defines a control point element as passed from the output
/// of the hull shader to the input of the domain shader
/////////////////////////////////////////////////////////////////////////
struct ScalarAttrib
{
    float x;
    float y;
    float z;
    float w;
};

struct ScalarCPoint
{
    ScalarAttrib attrib[KNOB_NUM_ATTRIBUTES];
};

//////////////////////////////////////////////////////////////////////////
/// SWR_TESSELLATION_FACTORS
/// @brief Tessellation factors structure (non-vector)
/////////////////////////////////////////////////////////////////////////
struct SWR_TESSELLATION_FACTORS
{
    float  OuterTessFactors[SWR_NUM_OUTER_TESS_FACTORS];
    float  InnerTessFactors[SWR_NUM_INNER_TESS_FACTORS];
};

#define MAX_NUM_VERTS_PER_PRIM 32 // support up to 32 control point patches
struct ScalarPatch
{
    SWR_TESSELLATION_FACTORS tessFactors;
    ScalarCPoint cp[MAX_NUM_VERTS_PER_PRIM];
    ScalarCPoint patchData;
};

//////////////////////////////////////////////////////////////////////////
/// SWR_HS_CONTEXT
/// @brief Input to hull shader
/////////////////////////////////////////////////////////////////////////
struct SWR_HS_CONTEXT
{
    simdvertex vert[MAX_NUM_VERTS_PER_PRIM]; // IN: (SIMD) input primitive data
    simdscalari PrimitiveID;    // IN: (SIMD) primitive ID generated from the draw call
    simdscalari mask;           // IN: Active mask for shader
    ScalarPatch* pCPout;        // OUT: Output control point patch
                                // SIMD-sized-array of SCALAR patches
};

//////////////////////////////////////////////////////////////////////////
/// SWR_DS_CONTEXT
/// @brief Input to domain shader
/////////////////////////////////////////////////////////////////////////
struct SWR_DS_CONTEXT
{
    uint32_t        PrimitiveID;    // IN: (SCALAR) PrimitiveID for the patch associated with the DS invocation
    uint32_t        vectorOffset;   // IN: (SCALAR) vector index offset into SIMD data.
    uint32_t        vectorStride;   // IN: (SCALAR) stride (in vectors) of output data per attribute-component
    ScalarPatch*    pCpIn;          // IN: (SCALAR) Control patch
    simdscalar*     pDomainU;       // IN: (SIMD) Domain Point U coords
    simdscalar*     pDomainV;       // IN: (SIMD) Domain Point V coords
    simdscalari     mask;           // IN: Active mask for shader
    simdscalar*     pOutputData;    // OUT: (SIMD) Vertex Attributes (2D array of vectors, one row per attribute-component)
};

//////////////////////////////////////////////////////////////////////////
/// SWR_GS_CONTEXT
/// @brief Input to geometry shader.
/////////////////////////////////////////////////////////////////////////
struct SWR_GS_CONTEXT
{
    simdvertex vert[MAX_NUM_VERTS_PER_PRIM]; // IN: input primitive data for SIMD prims
    simdscalari PrimitiveID;        // IN: input primitive ID generated from the draw call
    uint32_t InstanceID;            // IN: input instance ID
    simdscalari mask;               // IN: Active mask for shader
    uint8_t* pStream;               // OUT: output stream (contains vertices for all output streams)
    uint8_t* pCutOrStreamIdBuffer;  // OUT: cut or stream id buffer
    simdscalari vertexCount;        // OUT: num vertices emitted per SIMD lane
};

struct PixelPositions
{
    simdscalar UL;
    simdscalar center;
    simdscalar sample;
    simdscalar centroid;
};

#define SWR_MAX_NUM_MULTISAMPLES 16

//////////////////////////////////////////////////////////////////////////
/// SWR_PS_CONTEXT
/// @brief Input to pixel shader.
/////////////////////////////////////////////////////////////////////////
struct SWR_PS_CONTEXT
{
    PixelPositions vX;          // IN: x location(s) of pixels
    PixelPositions vY;          // IN: x location(s) of pixels
    simdscalar vZ;              // INOUT: z location of pixels
    simdscalari activeMask;     // OUT: mask for kill
    simdscalar  inputMask;      // IN: input coverage mask for all samples
    simdscalari oMask;          // OUT: mask for output coverage

    PixelPositions vI;          // barycentric coords evaluated at pixel center, sample position, centroid
    PixelPositions vJ;
    PixelPositions vOneOverW;   // IN: 1/w

    const float* pAttribs;      // IN: pointer to attribute barycentric coefficients
    const float* pPerspAttribs; // IN: pointer to attribute/w barycentric coefficients
    const float* pRecipW;       // IN: pointer to 1/w coord for each vertex
    const float *I;             // IN: Barycentric A, B, and C coefs used to compute I
    const float *J;             // IN: Barycentric A, B, and C coefs used to compute J
    float recipDet;             // IN: 1/Det, used when barycentric interpolating attributes
    const float* pSamplePosX;   // IN: array of sample positions
    const float* pSamplePosY;   // IN: array of sample positions
    simdvector shaded[SWR_NUM_RENDERTARGETS];
                                // OUT: result color per rendertarget

    uint32_t frontFace;         // IN: front- 1, back- 0
    uint32_t primID;            // IN: primitive ID
    uint32_t sampleIndex;       // IN: sampleIndex

};

//////////////////////////////////////////////////////////////////////////
/// SWR_CS_CONTEXT
/// @brief Input to compute shader.
/////////////////////////////////////////////////////////////////////////
struct SWR_CS_CONTEXT
{
    // The ThreadGroupId is the current thread group index relative
    // to all thread groups in the Dispatch call. The ThreadId, ThreadIdInGroup,
    // and ThreadIdInGroupFlattened can be derived from ThreadGroupId in the shader.

    // Compute shader accepts the following system values.
    // o ThreadId - Current thread id relative to all other threads in dispatch.
    // o ThreadGroupId - Current thread group id relative to all other groups in dispatch.
    // o ThreadIdInGroup - Current thread relative to all threads in the current thread group.
    // o ThreadIdInGroupFlattened - Flattened linear id derived from ThreadIdInGroup.
    //
    // All of these system values can be computed in the shader. They will be
    // derived from the current tile counter. The tile counter is an atomic counter that
    // resides in the draw context and is initialized to the product of the dispatch dims.
    //
    //  tileCounter = dispatchDims.x * dispatchDims.y * dispatchDims.z
    //
    // Each CPU worker thread will atomically decrement this counter and passes the current
    // count into the shader. When the count reaches 0 then all thread groups in the
    // dispatch call have been completed.

    uint32_t tileCounter;  // The tile counter value for this thread group.

    // Dispatch dimensions used by shader to compute system values from the tile counter.
    uint32_t dispatchDims[3];

    uint8_t* pTGSM;  // Thread Group Shared Memory pointer.

    uint8_t* pSpillFillBuffer;  // Spill/fill buffer for barrier support
};

// enums
enum SWR_TILE_MODE
{
    SWR_TILE_NONE = 0x0,    // Linear mode (no tiling)
    SWR_TILE_MODE_WMAJOR,   // W major tiling
    SWR_TILE_MODE_XMAJOR,   // X major tiling
    SWR_TILE_MODE_YMAJOR,   // Y major tiling
    SWR_TILE_SWRZ,          // SWR-Z tiling

    SWR_TILE_MODE_COUNT
};

enum SWR_SURFACE_TYPE
{
    SURFACE_1D        = 0,
    SURFACE_2D        = 1,
    SURFACE_3D        = 2,
    SURFACE_CUBE      = 3,
    SURFACE_BUFFER    = 4,
    SURFACE_STRUCTURED_BUFFER = 5,
    SURFACE_NULL       = 7
};

enum SWR_ZFUNCTION
{
    ZFUNC_ALWAYS,
    ZFUNC_NEVER,
    ZFUNC_LT,
    ZFUNC_EQ,
    ZFUNC_LE,
    ZFUNC_GT,
    ZFUNC_NE,
    ZFUNC_GE,
    NUM_ZFUNC
};

enum SWR_STENCILOP
{
    STENCILOP_KEEP,
    STENCILOP_ZERO,
    STENCILOP_REPLACE,
    STENCILOP_INCRSAT,
    STENCILOP_DECRSAT,
    STENCILOP_INCR,
    STENCILOP_DECR,
    STENCILOP_INVERT
};

enum SWR_BLEND_FACTOR
{
    BLENDFACTOR_ONE,
    BLENDFACTOR_SRC_COLOR,
    BLENDFACTOR_SRC_ALPHA,
    BLENDFACTOR_DST_ALPHA,
    BLENDFACTOR_DST_COLOR,
    BLENDFACTOR_SRC_ALPHA_SATURATE,
    BLENDFACTOR_CONST_COLOR,
    BLENDFACTOR_CONST_ALPHA,
    BLENDFACTOR_SRC1_COLOR,
    BLENDFACTOR_SRC1_ALPHA,
    BLENDFACTOR_ZERO,
    BLENDFACTOR_INV_SRC_COLOR,
    BLENDFACTOR_INV_SRC_ALPHA,
    BLENDFACTOR_INV_DST_ALPHA,
    BLENDFACTOR_INV_DST_COLOR,
    BLENDFACTOR_INV_CONST_COLOR,
    BLENDFACTOR_INV_CONST_ALPHA,
    BLENDFACTOR_INV_SRC1_COLOR,
    BLENDFACTOR_INV_SRC1_ALPHA
};

enum SWR_BLEND_OP
{
    BLENDOP_ADD,
    BLENDOP_SUBTRACT,
    BLENDOP_REVSUBTRACT,
    BLENDOP_MIN,
    BLENDOP_MAX,
};

enum SWR_LOGIC_OP
{
    LOGICOP_CLEAR,
    LOGICOP_NOR,
    LOGICOP_AND_INVERTED,
    LOGICOP_COPY_INVERTED,
    LOGICOP_AND_REVERSE,
    LOGICOP_INVERT,
    LOGICOP_XOR,
    LOGICOP_NAND,
    LOGICOP_AND,
    LOGICOP_EQUIV,
    LOGICOP_NOOP,
    LOGICOP_OR_INVERTED,
    LOGICOP_COPY,
    LOGICOP_OR_REVERSE,
    LOGICOP_OR,
    LOGICOP_SET,
};

struct SWR_SURFACE_STATE
{
    uint8_t *pBaseAddress;
    SWR_SURFACE_TYPE type;  // @llvm_enum
    SWR_FORMAT format;      // @llvm_enum
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t numSamples;
    uint32_t samplePattern;
    uint32_t pitch;
    uint32_t qpitch;
    uint32_t minLod;            // for sampled surfaces, the most detailed LOD that can be accessed by sampler
    uint32_t maxLod;            // for sampled surfaces, the max LOD that can be accessed
    float resourceMinLod;       // for sampled surfaces, the most detailed fractional mip that can be accessed by sampler
    uint32_t lod;               // for render targets, the lod being rendered to
    uint32_t arrayIndex;        // for render targets, the array index being rendered to for arrayed surfaces
    SWR_TILE_MODE tileMode;     // @llvm_enum
    bool bInterleavedSamples;   // are MSAA samples stored interleaved or planar
    uint32_t halign;
    uint32_t valign;
    uint32_t xOffset;
    uint32_t yOffset;

    uint32_t lodOffsets[2][15]; // lod offsets for sampled surfaces

    uint8_t *pAuxBaseAddress;   // Used for compression, append/consume counter, etc.
};

// vertex fetch state
// WARNING- any changes to this struct need to be reflected
// in the fetch shader jit
struct SWR_VERTEX_BUFFER_STATE
{
    uint32_t index;
    uint32_t pitch;
    const uint8_t *pData;
    uint32_t size;
    uint32_t numaNode;
    uint32_t maxVertex;             // size / pitch.  precalculated value used by fetch shader for OOB checks
    uint32_t partialInboundsSize;   // size % pitch.  precalculated value used by fetch shader for partially OOB vertices
};

struct SWR_INDEX_BUFFER_STATE
{
    // Format type for indices (e.g. UINT16, UINT32, etc.)
    SWR_FORMAT format; // @llvm_enum
    const void *pIndices;
    uint32_t size;
};


//////////////////////////////////////////////////////////////////////////
/// SWR_FETCH_CONTEXT
/// @brief Input to fetch shader.
/// @note WARNING - Changes to this struct need to be reflected in the
///                 fetch shader jit.
/////////////////////////////////////////////////////////////////////////
struct SWR_FETCH_CONTEXT
{
    const SWR_VERTEX_BUFFER_STATE* pStreams;    // IN: array of bound vertex buffers
    const int32_t* pIndices;                    // IN: pointer to index buffer for indexed draws
    const int32_t* pLastIndex;                  // IN: pointer to end of index buffer, used for bounds checking
    uint32_t CurInstance;                       // IN: current instance
    uint32_t BaseVertex;                        // IN: base vertex
    uint32_t StartVertex;                       // IN: start vertex
    uint32_t StartInstance;                     // IN: start instance
    simdscalari VertexID;                       // OUT: vector of vertex IDs
    simdscalari CutMask;                        // OUT: vector mask of indices which have the cut index value
};

//////////////////////////////////////////////////////////////////////////
/// SWR_STATS
///
/// @brief All statistics generated by SWR go here. These are public
///        to driver.
/////////////////////////////////////////////////////////////////////////
struct SWR_STATS
{
    // Occlusion Query
    uint64_t DepthPassCount; // Number of passing depth tests. Not exact.

    // Pipeline Stats
    uint64_t IaVertices;    // Number of Fetch Shader vertices
    uint64_t IaPrimitives;  // Number of PA primitives.
    uint64_t VsInvocations; // Number of Vertex Shader invocations
    uint64_t HsInvocations; // Number of Hull Shader invocations
    uint64_t DsInvocations; // Number of Domain Shader invocations
    uint64_t GsInvocations; // Number of Geometry Shader invocations
    uint64_t PsInvocations; // Number of Pixel Shader invocations
    uint64_t CsInvocations; // Number of Compute Shader invocations
    uint64_t CInvocations;  // Number of clipper invocations
    uint64_t CPrimitives;   // Number of clipper primitives.
    uint64_t GsPrimitives;  // Number of prims GS outputs.

    // Streamout Stats
    uint32_t SoWriteOffset[4];
    uint64_t SoPrimStorageNeeded[4];
    uint64_t SoNumPrimsWritten[4];
};

//////////////////////////////////////////////////////////////////////////
/// STREAMOUT_BUFFERS
/////////////////////////////////////////////////////////////////////////

#define MAX_SO_STREAMS 4
#define MAX_ATTRIBUTES 32

struct SWR_STREAMOUT_BUFFER
{
    bool enable;

    // Pointers to streamout buffers.
    uint32_t* pBuffer;

    // Size of buffer in dwords.
    uint32_t bufferSize;

    // Vertex pitch of buffer in dwords.
    uint32_t pitch;

    // Offset into buffer in dwords. SOS will increment this offset.
    uint32_t streamOffset;

    // Offset to the SO write offset. If not null then we update offset here.
    uint32_t* pWriteOffset;

};

//////////////////////////////////////////////////////////////////////////
/// STREAMOUT_STATE
/////////////////////////////////////////////////////////////////////////
struct SWR_STREAMOUT_STATE
{
    // This disables stream output.
    bool soEnable;

    // which streams are enabled for streamout
    bool streamEnable[MAX_SO_STREAMS];

    // If set then do not send any streams to the rasterizer.
    bool rasterizerDisable;

    // Specifies which stream to send to the rasterizer.
    uint32_t streamToRasterizer;

    // The stream masks specify which attributes are sent to which streams.
    // These masks help the FE to setup the pPrimData buffer that is passed
    // the the Stream Output Shader (SOS) function.
    uint32_t streamMasks[MAX_SO_STREAMS];

    // Number of attributes, including position, per vertex that are streamed out.
    // This should match number of bits in stream mask.
    uint32_t streamNumEntries[MAX_SO_STREAMS];
};

//////////////////////////////////////////////////////////////////////////
/// STREAMOUT_CONTEXT - Passed to SOS
/////////////////////////////////////////////////////////////////////////
struct SWR_STREAMOUT_CONTEXT
{
    uint32_t* pPrimData;
    SWR_STREAMOUT_BUFFER* pBuffer[MAX_SO_STREAMS];

    // Num prims written for this stream
    uint32_t numPrimsWritten;

    // Num prims that should have been written if there were no overflow.
    uint32_t numPrimStorageNeeded;
};

//////////////////////////////////////////////////////////////////////////
/// SWR_GS_STATE - Geometry shader state
/////////////////////////////////////////////////////////////////////////
struct SWR_GS_STATE
{
    bool gsEnable;

    // number of input attributes per vertex. used by the frontend to
    // optimize assembling primitives for GS
    uint32_t numInputAttribs;

    // output topology - can be point, tristrip, or linestrip
    PRIMITIVE_TOPOLOGY outputTopology;      // @llvm_enum

    // maximum number of verts that can be emitted by a single instance of the GS
    uint32_t maxNumVerts;
    
    // instance count
    uint32_t instanceCount;

    // geometry shader emits renderTargetArrayIndex
    bool emitsRenderTargetArrayIndex;

    // geometry shader emits PrimitiveID
    bool emitsPrimitiveID;

    // if true, geometry shader emits a single stream, with separate cut buffer.
    // if false, geometry shader emits vertices for multiple streams to the stream buffer, with a separate StreamID buffer
    // to map vertices to streams
    bool isSingleStream;

    // when single stream is enabled, singleStreamID dictates which stream is being output.
    // field ignored if isSingleStream is false
    uint32_t singleStreamID;
};


//////////////////////////////////////////////////////////////////////////
/// SWR_TS_OUTPUT_TOPOLOGY - Defines data output by the tessellator / DS
/////////////////////////////////////////////////////////////////////////
enum SWR_TS_OUTPUT_TOPOLOGY
{
    SWR_TS_OUTPUT_POINT,
    SWR_TS_OUTPUT_LINE,
    SWR_TS_OUTPUT_TRI_CW,
    SWR_TS_OUTPUT_TRI_CCW,

    SWR_TS_OUTPUT_TOPOLOGY_COUNT
};

//////////////////////////////////////////////////////////////////////////
/// SWR_TS_PARTITIONING - Defines tessellation algorithm
/////////////////////////////////////////////////////////////////////////
enum SWR_TS_PARTITIONING
{
    SWR_TS_INTEGER,
    SWR_TS_ODD_FRACTIONAL,
    SWR_TS_EVEN_FRACTIONAL,

    SWR_TS_PARTITIONING_COUNT
};

//////////////////////////////////////////////////////////////////////////
/// SWR_TS_DOMAIN - Defines Tessellation Domain
/////////////////////////////////////////////////////////////////////////
enum SWR_TS_DOMAIN
{
    SWR_TS_QUAD,
    SWR_TS_TRI,
    SWR_TS_ISOLINE,

    SWR_TS_DOMAIN_COUNT
};

//////////////////////////////////////////////////////////////////////////
/// SWR_TS_STATE - Tessellation state
/////////////////////////////////////////////////////////////////////////
struct SWR_TS_STATE
{
    bool                    tsEnable;
    SWR_TS_OUTPUT_TOPOLOGY  tsOutputTopology;   // @llvm_enum
    SWR_TS_PARTITIONING     partitioning;       // @llvm_enum
    SWR_TS_DOMAIN           domain;             // @llvm_enum

    PRIMITIVE_TOPOLOGY      postDSTopology;     // @llvm_enum

    uint32_t                numHsInputAttribs;
    uint32_t                numHsOutputAttribs;
    uint32_t                numDsOutputAttribs;
};

// output merger state
struct SWR_RENDER_TARGET_BLEND_STATE
{
    uint8_t writeDisableRed : 1;
    uint8_t writeDisableGreen : 1;
    uint8_t writeDisableBlue : 1;
    uint8_t writeDisableAlpha : 1;
};
static_assert(sizeof(SWR_RENDER_TARGET_BLEND_STATE) == 1, "Invalid SWR_RENDER_TARGET_BLEND_STATE size");

enum SWR_MULTISAMPLE_COUNT
{
    SWR_MULTISAMPLE_1X = 0,
    SWR_MULTISAMPLE_2X,
    SWR_MULTISAMPLE_4X,
    SWR_MULTISAMPLE_8X,
    SWR_MULTISAMPLE_16X,
    SWR_MULTISAMPLE_TYPE_MAX
};

struct SWR_BLEND_STATE
{
    // constant blend factor color in RGBA float
    float constantColor[4];

    // alpha test reference value in unorm8 or float32
    uint32_t alphaTestReference; 
    uint32_t sampleMask;
    // all RT's have the same sample count
    ///@todo move this to Output Merger state when we refactor
    SWR_MULTISAMPLE_COUNT sampleCount;  // @llvm_enum 

    SWR_RENDER_TARGET_BLEND_STATE renderTarget[SWR_NUM_RENDERTARGETS];
};
static_assert(sizeof(SWR_BLEND_STATE) == 36, "Invalid SWR_BLEND_STATE size");

//////////////////////////////////////////////////////////////////////////
/// FUNCTION POINTERS FOR SHADERS

typedef void(__cdecl *PFN_FETCH_FUNC)(SWR_FETCH_CONTEXT& fetchInfo, simdvertex& out);
typedef void(__cdecl *PFN_VERTEX_FUNC)(HANDLE hPrivateData, SWR_VS_CONTEXT* pVsContext);
typedef void(__cdecl *PFN_HS_FUNC)(HANDLE hPrivateData, SWR_HS_CONTEXT* pHsContext);
typedef void(__cdecl *PFN_DS_FUNC)(HANDLE hPrivateData, SWR_DS_CONTEXT* pDsContext);
typedef void(__cdecl *PFN_GS_FUNC)(HANDLE hPrivateData, SWR_GS_CONTEXT* pGsContext);
typedef void(__cdecl *PFN_CS_FUNC)(HANDLE hPrivateData, SWR_CS_CONTEXT* pCsContext);
typedef void(__cdecl *PFN_SO_FUNC)(SWR_STREAMOUT_CONTEXT& soContext);
typedef void(__cdecl *PFN_PIXEL_KERNEL)(HANDLE hPrivateData, SWR_PS_CONTEXT *pContext);
typedef void(__cdecl *PFN_CPIXEL_KERNEL)(HANDLE hPrivateData, SWR_PS_CONTEXT *pContext);
typedef void(__cdecl *PFN_BLEND_JIT_FUNC)(const SWR_BLEND_STATE*, simdvector&, simdvector&, uint32_t, uint8_t*, simdvector&, simdscalari*, simdscalari*);

//////////////////////////////////////////////////////////////////////////
/// FRONTEND_STATE
/////////////////////////////////////////////////////////////////////////
struct SWR_FRONTEND_STATE
{
    // skip clip test, perspective divide, and viewport transform
    // intended for verts in screen space
    bool vpTransformDisable;
    union
    {
        struct
        {
            uint32_t triFan : 2;
            uint32_t lineStripList : 1;
            uint32_t triStripList : 2;
        };
        uint32_t bits;
    }provokingVertex;
    uint32_t topologyProvokingVertex; // provoking vertex for the draw topology
};

//////////////////////////////////////////////////////////////////////////
/// VIEWPORT_MATRIX
/////////////////////////////////////////////////////////////////////////
struct SWR_VIEWPORT_MATRIX
{
    float m00;
    float m11;
    float m22;
    float m30;
    float m31;
    float m32;
};

//////////////////////////////////////////////////////////////////////////
/// SWR_VIEWPORT
/////////////////////////////////////////////////////////////////////////
struct SWR_VIEWPORT
{
    float x;
    float y;
    float width;
    float height;
    float minZ;
    float maxZ;
};

//////////////////////////////////////////////////////////////////////////
/// SWR_CULLMODE
//////////////////////////////////////////////////////////////////////////
enum SWR_CULLMODE
{
    SWR_CULLMODE_BOTH,
    SWR_CULLMODE_NONE,
    SWR_CULLMODE_FRONT,
    SWR_CULLMODE_BACK
};

enum SWR_FILLMODE
{
    SWR_FILLMODE_POINT,
    SWR_FILLMODE_WIREFRAME,
    SWR_FILLMODE_SOLID
};

enum SWR_FRONTWINDING
{
    SWR_FRONTWINDING_CW,
    SWR_FRONTWINDING_CCW
};


enum SWR_MSAA_SAMPLE_PATTERN
{
    SWR_MSAA_CENTER_PATTERN,
    SWR_MSAA_STANDARD_PATTERN,
    SWR_MSAA_SAMPLE_PATTERN_MAX
};

enum SWR_PIXEL_LOCATION
{
    SWR_PIXEL_LOCATION_CENTER,
    SWR_PIXEL_LOCATION_UL,
};

// fixed point screen space sample locations within a pixel
struct SWR_MULTISAMPLE_POS
{
    uint32_t x;
    uint32_t y;
};

enum SWR_MSAA_RASTMODE
{
    SWR_MSAA_RASTMODE_OFF_PIXEL,
    SWR_MSAA_RASTMODE_OFF_PATTERN,
    SWR_MSAA_RASTMODE_ON_PIXEL,
    SWR_MSAA_RASTMODE_ON_PATTERN
};

//////////////////////////////////////////////////////////////////////////
/// SWR_RASTSTATE
//////////////////////////////////////////////////////////////////////////
struct SWR_RASTSTATE
{
    uint32_t cullMode : 2;
    uint32_t fillMode : 2;
    uint32_t frontWinding : 1;
    uint32_t scissorEnable : 1;
    uint32_t depthClipEnable : 1;
    float pointSize;
    float lineWidth;

    // point size output from the VS
    bool pointParam;

    // point sprite
    bool pointSpriteEnable;
    bool pointSpriteTopOrigin;

    // depth bias
    float depthBias;
    float slopeScaledDepthBias;
    float depthBiasClamp;
    SWR_FORMAT depthFormat;     // @llvm_enum

    ///@todo: MSAA lines
    // multisample state for MSAA lines
    bool msaaRastEnable;
    SWR_MSAA_RASTMODE rastMode;    // @llvm_enum

    // sample count the rasterizer is running at
    SWR_MULTISAMPLE_COUNT sampleCount;  // @llvm_enum
    bool bForcedSampleCount;
    uint32_t pixelLocation;     // UL or Center
    bool pixelOffset;           // offset pixel positions by .5 in both the horizontal and vertical direction
    SWR_MULTISAMPLE_POS iSamplePos[SWR_MAX_NUM_MULTISAMPLES];   
    SWR_MSAA_SAMPLE_PATTERN samplePattern;   // @llvm_enum

    // user clip/cull distance enables
    uint8_t cullDistanceMask;
    uint8_t clipDistanceMask;
};

// backend state
struct SWR_BACKEND_STATE
{
    uint32_t constantInterpolationMask;
    uint32_t pointSpriteTexCoordMask;
    uint8_t numAttributes;
    uint8_t numComponents[KNOB_NUM_ATTRIBUTES];
};


union SWR_DEPTH_STENCIL_STATE
{
    struct
    {
        // dword 0
        uint32_t depthWriteEnable : 1;
        uint32_t depthTestEnable : 1;
        uint32_t stencilWriteEnable : 1;
        uint32_t stencilTestEnable : 1;
        uint32_t doubleSidedStencilTestEnable : 1;

        uint32_t depthTestFunc : 3;
        uint32_t stencilTestFunc : 3;

        uint32_t backfaceStencilPassDepthPassOp : 3;
        uint32_t backfaceStencilPassDepthFailOp : 3;
        uint32_t backfaceStencilFailOp : 3;
        uint32_t backfaceStencilTestFunc : 3;
        uint32_t stencilPassDepthPassOp : 3;
        uint32_t stencilPassDepthFailOp : 3;
        uint32_t stencilFailOp : 3;

        // dword 1
        uint8_t backfaceStencilWriteMask;
        uint8_t backfaceStencilTestMask;
        uint8_t stencilWriteMask;
        uint8_t stencilTestMask;

        // dword 2
        uint8_t backfaceStencilRefValue;
        uint8_t stencilRefValue;
    };
    uint32_t value[3];
};

enum SWR_SHADING_RATE
{
    SWR_SHADING_RATE_PIXEL,
    SWR_SHADING_RATE_SAMPLE,
    SWR_SHADING_RATE_MAX,
};

enum SWR_INPUT_COVERAGE
{
    SWR_INPUT_COVERAGE_NONE,
    SWR_INPUT_COVERAGE_NORMAL,
    SWR_INPUT_COVERAGE_MAX,
};

enum SWR_PS_POSITION_OFFSET
{
    SWR_PS_POSITION_SAMPLE_NONE, 
    SWR_PS_POSITION_SAMPLE_OFFSET, 
    SWR_PS_POSITION_CENTROID_OFFSET,
    SWR_PS_POSITION_OFFSET_MAX,
};

enum SWR_BARYCENTRICS_MASK
{
    SWR_BARYCENTRIC_PER_PIXEL_MASK = 0x1,
    SWR_BARYCENTRIC_CENTROID_MASK = 0x2,
    SWR_BARYCENTRIC_PER_SAMPLE_MASK = 0x4,
    SWR_BARYCENTRICS_MASK_MAX = 0x8
};

// pixel shader state
struct SWR_PS_STATE
{
    // dword 0-1
    PFN_PIXEL_KERNEL pfnPixelShader;  // @llvm_pfn

    // dword 2
    uint32_t killsPixel         : 1;    // pixel shader can kill pixels
    uint32_t inputCoverage      : 1;    // type of input coverage PS uses
    uint32_t writesODepth       : 1;    // pixel shader writes to depth
    uint32_t usesSourceDepth    : 1;    // pixel shader reads depth
    uint32_t shadingRate        : 2;    // shading per pixel / sample / coarse pixel
    uint32_t numRenderTargets   : 4;    // number of render target outputs in use (0-8)
    uint32_t posOffset          : 2;    // type of offset (none, sample, centroid) to add to pixel position
    uint32_t barycentricsMask   : 3;    // which type(s) of barycentric coords does the PS interpolate attributes with
    uint32_t usesUAV            : 1;    // pixel shader accesses UAV 
    uint32_t forceEarlyZ        : 1;    // force execution of early depth/stencil test

};
