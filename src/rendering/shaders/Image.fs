#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3( 0.0 )
#define MAX_IMAGE_TEXCOORD vec3( 1.0 )

// Rendering modes:
#define IMAGE_RENDER_MODE      0
#define CHECKER_RENDER_MODE    1
#define QUADRANTS_RENDER_MODE  2
#define FLASHLIGHT_RENDER_MODE 3

// Intensity Projection modes:
#define NO_IP_MODE   0
#define MAX_IP_MODE  1
#define MEAN_IP_MODE 2
#define MIN_IP_MODE  3

// Maximum number of isosurfaces:
#define NISO 16

// Redeclared vertex shader outputs, which are now the fragment shader inputs
in VS_OUT
{
    vec3 ImgTexCoords;
    vec3 SegTexCoords;
    vec2 CheckerCoord;
    vec2 ClipPos;
} fs_in;

layout (location = 0) out vec4 OutColor; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D imgTex; // Texture unit 0: image
uniform usampler3D segTex; // Texture unit 1: segmentation
uniform sampler1D imgCmapTex; // Texture unit 2: image color map (pre-mult RGBA)
uniform sampler1D segLabelCmapTex; // Texutre unit 3: label color map (pre-mult RGBA)

// Slope and intercept for mapping texture intensity to normalized intensity, accounting for window-leveling
uniform vec2 imgSlopeIntercept;

uniform vec2 imgCmapSlopeIntercept; // Slopes and intercepts for the image color maps

uniform vec2 imgThresholds; // Image lower and upper thresholds, mapped to OpenGL texture intensity
uniform float imgOpacity; // Image opacities
uniform float segOpacity; // Segmentation opacities

uniform bool masking; // Whether to mask image based on segmentation

uniform vec2 clipCrosshairs; // Crosshairs in Clip space

// Should comparison be done in x,y directions?
// If x is true, then compare along x; if y is true, then compare along y;
// if both are true, then compare along both.
uniform bvec2 quadrants;

// Should the fixed image be rendered (true) or the moving image (false):
uniform bool showFix;

// Render mode (0: normal, 1: checkerboard, 2: quadrants, 3: flashlight)
uniform int renderMode;

uniform float aspectRatio;

uniform float flashlightRadius;

// When true, the flashlight overlays the moving image on top of fixed image.
// When false, the flashlight replaces the fixed image with the moving image.
uniform bool flashlightOverlays;

// MIP mode (0: none, 1: max, 2: mean, 3: min, 4: x-ray)
uniform int mipMode;

// Half the number of samples for MIP. Set to 0 when no projection is used.
uniform int halfNumMipSamples;

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float segInteriorOpacity;

// Z view camera direction, represented in texture sampling space
uniform vec3 texSamplingDirZ;

uniform float isoValues[NISO]; // Isosurface values
uniform float isoOpacities[NISO]; // Isosurface opacities
uniform vec3 isoColors[NISO]; // Isosurface colors
uniform float isoWidth; // Width of isosurface


float hardThreshold( float value, vec2 thresholds )
{
    return float( thresholds[0] <= value && value <= thresholds[1] );
}

float cubicPulse( float center, float width, float x )
{
    x = abs( x - center );
    if ( x > width ) return 0.0;
    x /= width;
    return 1.0 - x * x * ( 3.0 - 2.0 * x );
}

// Check if inside texture coordinates
bool isInsideTexture( vec3 a )
{
    return ( all( greaterThanEqual( a, MIN_IMAGE_TEXCOORD ) ) &&
             all( lessThanEqual( a, MAX_IMAGE_TEXCOORD ) ) );
}


bool doRender()
{
    // Indicator of the quadrant of the crosshairs that the fragment is in:
    bvec2 Q = bvec2( fs_in.ClipPos.x <= clipCrosshairs.x,
                     fs_in.ClipPos.y > clipCrosshairs.y );

    // Distance of the fragment from the crosshairs, accounting for aspect ratio:
    float flashlightDist = sqrt(
        pow( aspectRatio * ( fs_in.ClipPos.x - clipCrosshairs.x ), 2.0 ) +
        pow( fs_in.ClipPos.y - clipCrosshairs.y, 2.0 ) );

    // Flag indicating whether the fragment will be rendered:
    bool render = ( IMAGE_RENDER_MODE == renderMode );

    // If in Checkerboard mode, then render the fragment?
    render = render || ( ( CHECKER_RENDER_MODE == renderMode ) &&
        ( showFix == bool( mod( floor( fs_in.CheckerCoord.x ) +
                                floor( fs_in.CheckerCoord.y ), 2.0 ) > 0.5 ) ) );

    // If in Quadrants mode, then render the fragment?
    render = render || ( ( QUADRANTS_RENDER_MODE == renderMode ) &&
        ( showFix == ( ( ! quadrants.x || Q.x ) == ( ! quadrants.y || Q.y ) ) ) );

    // If in Flashlight mode, then render the fragment?
    render = render || ( ( FLASHLIGHT_RENDER_MODE == renderMode ) &&
        ( ( showFix == ( flashlightDist > flashlightRadius ) ) ||
                       ( flashlightOverlays && showFix ) ) );

    return render;
}


// Compute alpha of fragments based on whether or not they are inside the
// segmentation boundary. Fragments on the boundary are assigned alpha of 1,
// whereas fragments inside are assigned alpha of 'segInteriorOpacity'.
float getSegInteriorAlpha( uint seg )
{
    float segInteriorAlpha = segInteriorOpacity;

    // Look up texture values in 8 neighbors surrounding the center fragment.
    // These may be either neighboring image voxels or neighboring view pixels.
    // The center fragment has index i = 4 (row = 0, col = 0).
    for ( int i = 0; i < 9; ++i )
    {
        float row = float( mod( i, 3 ) - 1 ); // runs -1 to 1
        float col = float( floor( i / 3 ) - 1 ); // runs -1 to 1

        vec3 texSamplingPos = row * texSamplingDirsForSegOutline[0] +
            col * texSamplingDirsForSegOutline[1];

        // Segmentation value of neighbor at (row, col) offset
        uint nseg = texture( segTex, fs_in.SegTexCoords + texSamplingPos )[0];

        // Fragment (with segmentation 'seg') is on the boundary (and hence gets
        // full alpha) if its value is not equal to one of its neighbors.
        if ( nseg != seg )
        {
            segInteriorAlpha = 1.0;
            break;
        }
    }

    return segInteriorAlpha;
}


void main()
{
    if ( ! doRender() ) discard;

    // Image and segmentation masks based on texture coordinates:
    bool imgMask = isInsideTexture( fs_in.ImgTexCoords );
    bool segMask = isInsideTexture( fs_in.SegTexCoords );

    // Look up the image value (after mapping to GL texture units):
    float img = texture( imgTex, fs_in.ImgTexCoords )[0];

    // Number of samples used for computing the final image value:
    int numSamples = 1;

    // Accumulate intensity projection in forwards (+Z) direction:
    for ( int i = 1; i <= halfNumMipSamples; ++i )
    {
        vec3 tc = fs_in.ImgTexCoords + i * texSamplingDirZ;
        if ( ! isInsideTexture( tc ) ) break;

        float a = texture( imgTex, tc )[0];

        img = float( MAX_IP_MODE == mipMode ) * max( img, a ) +
              float( MEAN_IP_MODE == mipMode ) * ( img + a ) +
              float( MIN_IP_MODE == mipMode ) * min( img, a );

        ++numSamples;
    }

    // Accumulate intensity projection in backwards (-Z) direction:
    for ( int i = 1; i <= halfNumMipSamples; ++i )
    {
        vec3 tc = fs_in.ImgTexCoords - i * texSamplingDirZ;
        if ( ! isInsideTexture( tc ) ) break;

        float a = texture( imgTex, tc )[0];

        img = float( MAX_IP_MODE == mipMode ) * max( img, a ) +
              float( MEAN_IP_MODE == mipMode ) * ( img + a ) +
              float( MIN_IP_MODE == mipMode ) * min( img, a );

        ++numSamples;
    }

    // If using Mean Intensity Projection mode, then normalize by the number of samples:
    img /= mix( 1.0, float( numSamples ), float( MEAN_IP_MODE == mipMode ) );

    // Look up segmentation texture label value:
    uint seg = texture( segTex, fs_in.SegTexCoords )[0];

    // Apply window/level to normalize image values in [0.0, 1.0] range:
    float imgNorm = clamp( imgSlopeIntercept[0] * img + imgSlopeIntercept[1], 0.0, 1.0 );

    // Compute the image mask:
    float mask = float( imgMask && ( masking && ( seg > 0u ) || ! masking ) );

    // Compute image alpha based on opacity, mask, and thresholds:
    float imgAlpha = imgOpacity * mask * hardThreshold( img, imgThresholds );

    // Compute segmentation alpha based on opacity and mask:
    float segAlpha = segOpacity * getSegInteriorAlpha( seg ) * float(segMask);

    // Look up image color and apply alpha:
    vec4 imgLayer = texture( imgCmapTex, imgCmapSlopeIntercept[0] * imgNorm + imgCmapSlopeIntercept[1] ) * imgAlpha;

    // Look up segmentation color and apply:
    vec4 segLayer = texelFetch( segLabelCmapTex, int(seg), 0 ) * segAlpha;

    // Isosurface layer:
    vec4 isoLayer = vec4(0.0, 0.0, 0.0, 0.0);

    for ( int i = 0; i < NISO; ++i )
    {
        vec4 color = float(imgMask) * isoOpacities[i] * cubicPulse( isoValues[i], isoWidth, img ) * vec4( isoColors[i], 1.0 );
        isoLayer = color + (1.0 - color.a) * isoLayer;
    }

    // Blend layers:
    OutColor = vec4(0.0, 0.0, 0.0, 0.0);
    OutColor = imgLayer + (1.0 - imgLayer.a) * OutColor;
    OutColor = isoLayer + (1.0 - isoLayer.a) * OutColor;
    OutColor = segLayer + (1.0 - segLayer.a) * OutColor;

    //OutColor.rgb = pow( OutColor.rgb, vec3(1.8) );
}
