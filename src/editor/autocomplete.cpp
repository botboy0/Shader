#include "autocomplete.h"

#include <algorithm>
#include <cctype>

namespace magnetar {

// ---------------------------------------------------------------------------
// Master completion dictionary — 170+ items
// Organized by: GLSL functions, types, keywords, qualifiers, Shadertoy uniforms
// ---------------------------------------------------------------------------

static std::vector<CompletionItem> buildItems()
{
    using Cat = CompletionCategory;
    std::vector<CompletionItem> items;
    items.reserve(200);

    // ── Trigonometric functions ──────────────────────────────────────────
    items.push_back({"sin",       "float sin(float x)",           Cat::Function});
    items.push_back({"cos",       "float cos(float x)",           Cat::Function});
    items.push_back({"tan",       "float tan(float x)",           Cat::Function});
    items.push_back({"asin",      "float asin(float x)",          Cat::Function});
    items.push_back({"acos",      "float acos(float x)",          Cat::Function});
    items.push_back({"atan",      "float atan(float y, float x)", Cat::Function});
    items.push_back({"sinh",      "float sinh(float x)",          Cat::Function});
    items.push_back({"cosh",      "float cosh(float x)",          Cat::Function});
    items.push_back({"tanh",      "float tanh(float x)",          Cat::Function});
    items.push_back({"asinh",     "float asinh(float x)",         Cat::Function});
    items.push_back({"acosh",     "float acosh(float x)",         Cat::Function});
    items.push_back({"atanh",     "float atanh(float x)",         Cat::Function});

    // ── Exponential / power ─────────────────────────────────────────────
    items.push_back({"pow",          "float pow(float x, float y)",  Cat::Function});
    items.push_back({"exp",          "float exp(float x)",           Cat::Function});
    items.push_back({"log",          "float log(float x)",           Cat::Function});
    items.push_back({"exp2",         "float exp2(float x)",          Cat::Function});
    items.push_back({"log2",         "float log2(float x)",          Cat::Function});
    items.push_back({"sqrt",         "float sqrt(float x)",          Cat::Function});
    items.push_back({"inversesqrt",  "float inversesqrt(float x)",   Cat::Function});

    // ── Common math ─────────────────────────────────────────────────────
    items.push_back({"abs",         "float abs(float x)",                         Cat::Function});
    items.push_back({"sign",        "float sign(float x)",                        Cat::Function});
    items.push_back({"floor",       "float floor(float x)",                       Cat::Function});
    items.push_back({"ceil",        "float ceil(float x)",                        Cat::Function});
    items.push_back({"round",       "float round(float x)",                       Cat::Function});
    items.push_back({"roundEven",   "float roundEven(float x)",                   Cat::Function});
    items.push_back({"trunc",       "float trunc(float x)",                       Cat::Function});
    items.push_back({"fract",       "float fract(float x)",                       Cat::Function});
    items.push_back({"mod",         "float mod(float x, float y)",                Cat::Function});
    items.push_back({"modf",        "float modf(float x, out float i)",           Cat::Function});
    items.push_back({"min",         "float min(float x, float y)",                Cat::Function});
    items.push_back({"max",         "float max(float x, float y)",                Cat::Function});
    items.push_back({"clamp",       "float clamp(float x, float lo, float hi)",   Cat::Function});
    items.push_back({"mix",         "float mix(float x, float y, float a)",       Cat::Function});
    items.push_back({"step",        "float step(float edge, float x)",            Cat::Function});
    items.push_back({"smoothstep",  "float smoothstep(float e0, float e1, float x)", Cat::Function});
    items.push_back({"isnan",       "bool isnan(float x)",                        Cat::Function});
    items.push_back({"isinf",       "bool isinf(float x)",                        Cat::Function});
    items.push_back({"fma",         "float fma(float a, float b, float c)",       Cat::Function});

    // ── Geometric ───────────────────────────────────────────────────────
    items.push_back({"length",      "float length(vec x)",            Cat::Function});
    items.push_back({"distance",    "float distance(vec p0, vec p1)", Cat::Function});
    items.push_back({"dot",         "float dot(vec x, vec y)",        Cat::Function});
    items.push_back({"cross",       "vec3 cross(vec3 x, vec3 y)",     Cat::Function});
    items.push_back({"normalize",   "vec normalize(vec x)",           Cat::Function});
    items.push_back({"reflect",     "vec reflect(vec I, vec N)",      Cat::Function});
    items.push_back({"refract",     "vec refract(vec I, vec N, float eta)", Cat::Function});
    items.push_back({"faceforward", "vec faceforward(vec N, vec I, vec Nref)", Cat::Function});

    // ── Matrix ──────────────────────────────────────────────────────────
    items.push_back({"matrixCompMult", "mat matrixCompMult(mat x, mat y)", Cat::Function});
    items.push_back({"transpose",      "mat transpose(mat m)",            Cat::Function});
    items.push_back({"inverse",        "mat inverse(mat m)",              Cat::Function});
    items.push_back({"determinant",    "float determinant(mat m)",        Cat::Function});
    items.push_back({"outerProduct",   "mat outerProduct(vec c, vec r)",  Cat::Function});

    // ── Relational / logic ──────────────────────────────────────────────
    items.push_back({"lessThan",     "bvec lessThan(vec x, vec y)",     Cat::Function});
    items.push_back({"greaterThan",  "bvec greaterThan(vec x, vec y)",  Cat::Function});
    items.push_back({"lessThanEqual","bvec lessThanEqual(vec x, vec y)",Cat::Function});
    items.push_back({"greaterThanEqual","bvec greaterThanEqual(vec x, vec y)", Cat::Function});
    items.push_back({"equal",        "bvec equal(vec x, vec y)",        Cat::Function});
    items.push_back({"notEqual",     "bvec notEqual(vec x, vec y)",     Cat::Function});
    items.push_back({"any",          "bool any(bvec x)",                Cat::Function});
    items.push_back({"all",          "bool all(bvec x)",                Cat::Function});
    items.push_back({"not",          "bvec not(bvec x)",                Cat::Function});

    // ── Texture ─────────────────────────────────────────────────────────
    items.push_back({"texture",          "vec4 texture(sampler s, vec coord)",                Cat::Function});
    items.push_back({"textureLod",       "vec4 textureLod(sampler s, vec coord, float lod)",  Cat::Function});
    items.push_back({"textureOffset",    "vec4 textureOffset(sampler s, vec coord, ivec off)", Cat::Function});
    items.push_back({"texelFetch",       "vec4 texelFetch(sampler s, ivec coord, int lod)",   Cat::Function});
    items.push_back({"textureGrad",      "vec4 textureGrad(sampler s, vec P, vec dPdx, vec dPdy)", Cat::Function});
    items.push_back({"textureSize",      "ivec textureSize(sampler s, int lod)",              Cat::Function});
    items.push_back({"textureLodOffset", "vec4 textureLodOffset(sampler s, vec P, float lod, ivec off)", Cat::Function});
    items.push_back({"textureProj",      "vec4 textureProj(sampler s, vec P)",                Cat::Function});
    items.push_back({"textureProjLod",   "vec4 textureProjLod(sampler s, vec P, float lod)",  Cat::Function});
    items.push_back({"textureGather",    "vec4 textureGather(sampler s, vec P)",               Cat::Function});

    // ── Derivative ──────────────────────────────────────────────────────
    items.push_back({"dFdx",     "float dFdx(float p)",   Cat::Function});
    items.push_back({"dFdy",     "float dFdy(float p)",   Cat::Function});
    items.push_back({"fwidth",   "float fwidth(float p)",  Cat::Function});
    items.push_back({"dFdxFine", "float dFdxFine(float p)", Cat::Function});
    items.push_back({"dFdyFine", "float dFdyFine(float p)", Cat::Function});
    items.push_back({"dFdxCoarse","float dFdxCoarse(float p)", Cat::Function});
    items.push_back({"dFdyCoarse","float dFdyCoarse(float p)", Cat::Function});
    items.push_back({"fwidthFine","float fwidthFine(float p)", Cat::Function});
    items.push_back({"fwidthCoarse","float fwidthCoarse(float p)", Cat::Function});

    // ── Packing / bit manipulation ──────────────────────────────────────
    items.push_back({"floatBitsToInt",   "int floatBitsToInt(float v)",      Cat::Function});
    items.push_back({"floatBitsToUint",  "uint floatBitsToUint(float v)",    Cat::Function});
    items.push_back({"intBitsToFloat",   "float intBitsToFloat(int v)",      Cat::Function});
    items.push_back({"uintBitsToFloat",  "float uintBitsToFloat(uint v)",    Cat::Function});
    items.push_back({"packSnorm2x16",    "uint packSnorm2x16(vec2 v)",       Cat::Function});
    items.push_back({"unpackSnorm2x16",  "vec2 unpackSnorm2x16(uint p)",     Cat::Function});
    items.push_back({"packUnorm2x16",    "uint packUnorm2x16(vec2 v)",       Cat::Function});
    items.push_back({"unpackUnorm2x16",  "vec2 unpackUnorm2x16(uint p)",     Cat::Function});
    items.push_back({"packHalf2x16",     "uint packHalf2x16(vec2 v)",        Cat::Function});
    items.push_back({"unpackHalf2x16",   "vec2 unpackHalf2x16(uint p)",      Cat::Function});
    items.push_back({"bitfieldExtract",  "int bitfieldExtract(int v, int off, int bits)",  Cat::Function});
    items.push_back({"bitfieldInsert",   "int bitfieldInsert(int base, int ins, int off, int bits)", Cat::Function});
    items.push_back({"bitfieldReverse",  "int bitfieldReverse(int v)",        Cat::Function});
    items.push_back({"bitCount",         "int bitCount(int v)",               Cat::Function});
    items.push_back({"findLSB",          "int findLSB(int v)",                Cat::Function});
    items.push_back({"findMSB",          "int findMSB(int v)",                Cat::Function});

    // ── Image ───────────────────────────────────────────────────────────
    items.push_back({"imageLoad",   "vec4 imageLoad(image2D img, ivec2 P)",           Cat::Function});
    items.push_back({"imageStore",  "void imageStore(image2D img, ivec2 P, vec4 d)",  Cat::Function});
    items.push_back({"imageSize",   "ivec2 imageSize(image2D img)",                   Cat::Function});

    // ── Atomic / barrier ────────────────────────────────────────────────
    items.push_back({"barrier",       "void barrier()",                  Cat::Function});
    items.push_back({"memoryBarrier", "void memoryBarrier()",            Cat::Function});
    items.push_back({"atomicAdd",     "int atomicAdd(inout int mem, int data)", Cat::Function});
    items.push_back({"atomicMin",     "int atomicMin(inout int mem, int data)", Cat::Function});
    items.push_back({"atomicMax",     "int atomicMax(inout int mem, int data)", Cat::Function});
    items.push_back({"atomicAnd",     "int atomicAnd(inout int mem, int data)", Cat::Function});
    items.push_back({"atomicOr",      "int atomicOr(inout int mem, int data)",  Cat::Function});
    items.push_back({"atomicXor",     "int atomicXor(inout int mem, int data)", Cat::Function});
    items.push_back({"atomicExchange","int atomicExchange(inout int mem, int data)", Cat::Function});
    items.push_back({"atomicCompSwap","int atomicCompSwap(inout int mem, int cmp, int data)", Cat::Function});

    // ── Scalar / vector types ───────────────────────────────────────────
    items.push_back({"void",    "void",    Cat::Type});
    items.push_back({"bool",    "bool",    Cat::Type});
    items.push_back({"int",     "int",     Cat::Type});
    items.push_back({"uint",    "uint",    Cat::Type});
    items.push_back({"float",   "float",   Cat::Type});
    items.push_back({"double",  "double",  Cat::Type});
    items.push_back({"vec2",    "vec2",    Cat::Type});
    items.push_back({"vec3",    "vec3",    Cat::Type});
    items.push_back({"vec4",    "vec4",    Cat::Type});
    items.push_back({"ivec2",   "ivec2",   Cat::Type});
    items.push_back({"ivec3",   "ivec3",   Cat::Type});
    items.push_back({"ivec4",   "ivec4",   Cat::Type});
    items.push_back({"uvec2",   "uvec2",   Cat::Type});
    items.push_back({"uvec3",   "uvec3",   Cat::Type});
    items.push_back({"uvec4",   "uvec4",   Cat::Type});
    items.push_back({"bvec2",   "bvec2",   Cat::Type});
    items.push_back({"bvec3",   "bvec3",   Cat::Type});
    items.push_back({"bvec4",   "bvec4",   Cat::Type});
    items.push_back({"dvec2",   "dvec2",   Cat::Type});
    items.push_back({"dvec3",   "dvec3",   Cat::Type});
    items.push_back({"dvec4",   "dvec4",   Cat::Type});
    items.push_back({"mat2",    "mat2",    Cat::Type});
    items.push_back({"mat3",    "mat3",    Cat::Type});
    items.push_back({"mat4",    "mat4",    Cat::Type});
    items.push_back({"mat2x3",  "mat2x3",  Cat::Type});
    items.push_back({"mat2x4",  "mat2x4",  Cat::Type});
    items.push_back({"mat3x2",  "mat3x2",  Cat::Type});
    items.push_back({"mat3x4",  "mat3x4",  Cat::Type});
    items.push_back({"mat4x2",  "mat4x2",  Cat::Type});
    items.push_back({"mat4x3",  "mat4x3",  Cat::Type});
    items.push_back({"dmat2",   "dmat2",   Cat::Type});
    items.push_back({"dmat3",   "dmat3",   Cat::Type});
    items.push_back({"dmat4",   "dmat4",   Cat::Type});

    // ── Sampler / image types ───────────────────────────────────────────
    items.push_back({"sampler2D",          "sampler2D",          Cat::Type});
    items.push_back({"sampler3D",          "sampler3D",          Cat::Type});
    items.push_back({"samplerCube",        "samplerCube",        Cat::Type});
    items.push_back({"sampler2DShadow",    "sampler2DShadow",    Cat::Type});
    items.push_back({"samplerCubeShadow",  "samplerCubeShadow",  Cat::Type});
    items.push_back({"sampler2DArray",     "sampler2DArray",     Cat::Type});
    items.push_back({"isampler2D",         "isampler2D",         Cat::Type});
    items.push_back({"isampler3D",         "isampler3D",         Cat::Type});
    items.push_back({"usampler2D",         "usampler2D",         Cat::Type});
    items.push_back({"usampler3D",         "usampler3D",         Cat::Type});
    items.push_back({"image2D",            "image2D",            Cat::Type});

    // ── Keywords ────────────────────────────────────────────────────────
    items.push_back({"struct",    "struct",    Cat::Keyword});
    items.push_back({"if",        "if",        Cat::Keyword});
    items.push_back({"else",      "else",      Cat::Keyword});
    items.push_back({"for",       "for",       Cat::Keyword});
    items.push_back({"while",     "while",     Cat::Keyword});
    items.push_back({"do",        "do",        Cat::Keyword});
    items.push_back({"switch",    "switch",    Cat::Keyword});
    items.push_back({"case",      "case",      Cat::Keyword});
    items.push_back({"default",   "default",   Cat::Keyword});
    items.push_back({"break",     "break",     Cat::Keyword});
    items.push_back({"continue",  "continue",  Cat::Keyword});
    items.push_back({"return",    "return",    Cat::Keyword});
    items.push_back({"discard",   "discard",   Cat::Keyword});
    items.push_back({"true",      "true",      Cat::Keyword});
    items.push_back({"false",     "false",     Cat::Keyword});

    // ── Qualifiers / storage ────────────────────────────────────────────
    items.push_back({"uniform",       "uniform",       Cat::Qualifier});
    items.push_back({"in",            "in",            Cat::Qualifier});
    items.push_back({"out",           "out",           Cat::Qualifier});
    items.push_back({"inout",         "inout",         Cat::Qualifier});
    items.push_back({"layout",        "layout",        Cat::Qualifier});
    items.push_back({"flat",          "flat",          Cat::Qualifier});
    items.push_back({"smooth",        "smooth",        Cat::Qualifier});
    items.push_back({"noperspective", "noperspective", Cat::Qualifier});
    items.push_back({"centroid",      "centroid",      Cat::Qualifier});
    items.push_back({"const",         "const",         Cat::Qualifier});
    items.push_back({"coherent",      "coherent",      Cat::Qualifier});
    items.push_back({"volatile",      "volatile",      Cat::Qualifier});
    items.push_back({"restrict",      "restrict",      Cat::Qualifier});
    items.push_back({"readonly",      "readonly",      Cat::Qualifier});
    items.push_back({"writeonly",     "writeonly",      Cat::Qualifier});
    items.push_back({"buffer",        "buffer",        Cat::Qualifier});
    items.push_back({"shared",        "shared",        Cat::Qualifier});
    items.push_back({"precision",     "precision",     Cat::Qualifier});
    items.push_back({"highp",         "highp",         Cat::Qualifier});
    items.push_back({"mediump",       "mediump",       Cat::Qualifier});
    items.push_back({"lowp",          "lowp",          Cat::Qualifier});

    // ── GLSL built-in variables ─────────────────────────────────────────
    items.push_back({"gl_FragCoord",    "vec4 gl_FragCoord",    Cat::Variable});
    items.push_back({"gl_FragColor",    "vec4 gl_FragColor",    Cat::Variable});
    items.push_back({"gl_Position",     "vec4 gl_Position",     Cat::Variable});
    items.push_back({"gl_VertexID",     "int gl_VertexID",      Cat::Variable});
    items.push_back({"gl_InstanceID",   "int gl_InstanceID",    Cat::Variable});
    items.push_back({"gl_FrontFacing",  "bool gl_FrontFacing",  Cat::Variable});
    items.push_back({"gl_PointSize",    "float gl_PointSize",   Cat::Variable});
    items.push_back({"gl_FragDepth",    "float gl_FragDepth",   Cat::Variable});

    // ── Shadertoy uniforms ──────────────────────────────────────────────
    items.push_back({"iTime",              "uniform float iTime",                Cat::Uniform});
    items.push_back({"iTimeDelta",         "uniform float iTimeDelta",           Cat::Uniform});
    items.push_back({"iFrame",             "uniform int iFrame",                 Cat::Uniform});
    items.push_back({"iResolution",        "uniform vec3 iResolution",           Cat::Uniform});
    items.push_back({"iMouse",             "uniform vec4 iMouse",                Cat::Uniform});
    items.push_back({"iDate",              "uniform vec4 iDate",                 Cat::Uniform});
    items.push_back({"iChannel0",          "uniform sampler2D iChannel0",        Cat::Uniform});
    items.push_back({"iChannel1",          "uniform sampler2D iChannel1",        Cat::Uniform});
    items.push_back({"iChannel2",          "uniform sampler2D iChannel2",        Cat::Uniform});
    items.push_back({"iChannel3",          "uniform sampler2D iChannel3",        Cat::Uniform});
    items.push_back({"iChannelResolution", "uniform vec3 iChannelResolution[4]", Cat::Uniform});

    // ── Shadertoy entry points ──────────────────────────────────────────
    items.push_back({"mainImage",  "void mainImage(out vec4 fragColor, in vec2 fragCoord)", Cat::Function});
    items.push_back({"fragCoord",  "vec2 fragCoord",  Cat::Variable});

    // Sort alphabetically for consistent display and binary search
    std::sort(items.begin(), items.end(),
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.name < b.name;
        });

    return items;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

const std::vector<CompletionItem>& GlslAutocomplete::getItems()
{
    static const std::vector<CompletionItem> items = buildItems();
    return items;
}

std::vector<const CompletionItem*> GlslAutocomplete::filter(const std::string& prefix)
{
    std::vector<const CompletionItem*> results;
    if (prefix.empty()) return results;

    // Lowercase prefix for case-insensitive matching
    std::string lowerPrefix = prefix;
    for (auto& c : lowerPrefix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    const auto& items = getItems();
    for (const auto& item : items) {
        if (item.name.size() < prefix.size()) continue;

        // Case-insensitive prefix comparison
        bool match = true;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(item.name[i])) != lowerPrefix[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            results.push_back(&item);
        }
    }

    // Already sorted alphabetically from the static list
    return results;
}

} // namespace magnetar
