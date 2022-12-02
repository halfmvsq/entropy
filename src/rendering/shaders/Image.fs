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
    vec3 SegVoxCoords;
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

uniform bool useTricubicInterpolation; // Whether to use tricubic interpolation

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


//! Tricubic interpolated texture lookup, using unnormalized coordinates.
//! Fast implementation, using 8 trilinear lookups.
//! @param[in] tex  3D texture
//! @param[in] coord  normalized 3D texture coordinate
//! @see https://github.com/DannyRuijters/CubicInterpolationCUDA/blob/master/examples/glCubicRayCast/tricubic.shader
float interpolateTricubicFast( sampler3D tex, vec3 coord )
{
	// Shift the coordinate from [0,1] to [-0.5, nrOfVoxels-0.5]
    vec3 nrOfVoxels = vec3(textureSize(tex, 0));
    vec3 coord_grid = coord * nrOfVoxels - 0.5;
    vec3 index = floor(coord_grid);
    vec3 fraction = coord_grid - index;
    vec3 one_frac = 1.0 - fraction;

    vec3 w0 = 1.0/6.0 * one_frac*one_frac*one_frac;
    vec3 w1 = 2.0/3.0 - 0.5 * fraction*fraction*(2.0-fraction);
    vec3 w2 = 2.0/3.0 - 0.5 * one_frac*one_frac*(2.0-one_frac);
    vec3 w3 = 1.0/6.0 * fraction*fraction*fraction;

    vec3 g0 = w0 + w1;
    vec3 g1 = w2 + w3;
    vec3 mult = 1.0 / nrOfVoxels;

    // h0 = w1/g0 - 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
    vec3 h0 = mult * ((w1 / g0) - 0.5 + index);

    // h1 = w3/g1 + 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
    vec3 h1 = mult * ((w3 / g1) + 1.5 + index);

	// Fetch the eight linear interpolations
	// weighting and fetching is interleaved for performance and stability reasons
    float tex000 = texture(tex, h0)[0];
    float tex100 = texture(tex, vec3(h1.x, h0.y, h0.z))[0];

    tex000 = mix(tex100, tex000, g0.x); // weigh along the x-direction
    float tex010 = texture(tex, vec3(h0.x, h1.y, h0.z))[0];
    float tex110 = texture(tex, vec3(h1.x, h1.y, h0.z))[0];

    tex010 = mix(tex110, tex010, g0.x); // weigh along the x-direction
    tex000 = mix(tex010, tex000, g0.y); // weigh along the y-direction

    float tex001 = texture(tex, vec3(h0.x, h0.y, h1.z))[0];
    float tex101 = texture(tex, vec3(h1.x, h0.y, h1.z))[0];

    tex001 = mix(tex101, tex001, g0.x); // weigh along the x-direction

    float tex011 = texture(tex, vec3(h0.x, h1.y, h1.z))[0];
    float tex111 = texture(tex, h1)[0];

    tex011 = mix(tex111, tex011, g0.x); // weigh along the x-direction
    tex001 = mix(tex011, tex001, g0.y); // weigh along the y-direction

    return mix(tex001, tex000, g0.z); // weigh along the z-direction
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
        float col = float( floor( float(i / 3) ) - 1 ); // runs -1 to 1

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


float getImageValue( vec3 texCoord )
{
    return mix( texture( imgTex, texCoord )[0],
        interpolateTricubicFast( imgTex, texCoord ),
        float(useTricubicInterpolation) );
}


void main()
{
    if ( ! doRender() ) discard;

    // Image and segmentation masks based on texture coordinates:
    bool imgMask = isInsideTexture( fs_in.ImgTexCoords );
    bool segMask = isInsideTexture( fs_in.SegTexCoords );

    // Look up the image value (after mapping to GL texture units):
    //float img = texture( imgTex, fs_in.ImgTexCoords )[0];
    float img = getImageValue( fs_in.ImgTexCoords );

    // Number of samples used for computing the final image value:
    int numSamples = 1;

    // Accumulate intensity projection in forwards (+Z) direction:
    for ( int i = 1; i <= halfNumMipSamples; ++i )
    {
        vec3 tc = fs_in.ImgTexCoords + i * texSamplingDirZ;
        if ( ! isInsideTexture( tc ) ) break;

        float a = getImageValue( tc );

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

        float a = getImageValue( tc );

        img = float( MAX_IP_MODE == mipMode ) * max( img, a ) +
              float( MEAN_IP_MODE == mipMode ) * ( img + a ) +
              float( MIN_IP_MODE == mipMode ) * min( img, a );

        ++numSamples;
    }

    // If using Mean Intensity Projection mode, then normalize by the number of samples:
    img /= mix( 1.0, float( numSamples ), float( MEAN_IP_MODE == mipMode ) );

    // Look up segmentation texture label value:
    uint seg = texture( segTex, fs_in.SegTexCoords )[0];

    vec3 c = floor( fs_in.SegVoxCoords );

    vec3 td = pow( vec3( textureSize(segTex, 0) ), vec3(-1) );
    vec3 tc = vec3( c.x * td.x, c.y * td.y, c.z * td.z ) + 0.5 * td;

    uint s000 = texture( segTex, vec3( tc.x, tc.y, tc.z ) )[0];
    uint s001 = texture( segTex, vec3( tc.x, tc.y, tc.z + td.z ) )[0];
    uint s010 = texture( segTex, vec3( tc.x, tc.y + td.y, tc.z ) )[0];
    uint s011 = texture( segTex, vec3( tc.x, tc.y + td.y, tc.z + td.z ) )[0];
    uint s100 = texture( segTex, vec3( tc.x + td.x, tc.y, tc.z ) )[0];
    uint s101 = texture( segTex, vec3( tc.x + td.x, tc.y, tc.z + td.z ) )[0];
    uint s110 = texture( segTex, vec3( tc.x + td.x, tc.y + td.y, tc.z ) )[0];
    uint s111 = texture( segTex, vec3( tc.x + td.x, tc.y + td.y, tc.z + td.z ) )[0];
    
    seg = 0u;
    float segInterpOpacity = 0.0;

    vec3 d = fs_in.SegVoxCoords - c;
    vec3 e = vec3(1) - d;

    for ( uint j = 1u; j <= 4u; ++j )
    {
        float h =
            float(s000 == j) * e.x * e.y * e.z +
            float(s001 == j) * e.x * e.y * d.z + 
            float(s010 == j) * e.x * d.y * e.z + 
            float(s011 == j) * e.x * d.y * d.z + 
            float(s100 == j) * d.x * e.y * e.z + 
            float(s101 == j) * d.x * e.y * d.z + 
            float(s110 == j) * d.x * d.y * e.z + 
            float(s111 == j) * d.x * d.y * d.z;

        // segInterpOpacity = 1.0;
        // seg = uint( mix( 0u, j, float(h > isoOpacities[0]) ) );

        segInterpOpacity = cubicPulse( isoOpacities[0], 0.1, h );
        seg = j;

        // if ( h > isoOpacities[0] )
        // {
        //     seg = j;
        //     segInterpOpacity = 1.0;
        // }
        // else
        // {
        //     seg = 0u;
        //     segInterpOpacity = 0.0;
        // }

        if ( h > 0.0 )
        {
            break;
        }
    }

    // seg = uint( mix( 0u, 1u, float(segVal >= 0.5) ) );

    // Apply window/level to normalize image values in [0.0, 1.0] range:
    float imgNorm = clamp( imgSlopeIntercept[0] * img + imgSlopeIntercept[1], 0.0, 1.0 );

    // Compute the image mask:
    float mask = float( imgMask && ( masking && ( seg > 0u ) || ! masking ) );

    // Compute image alpha based on opacity, mask, and thresholds:
    float imgAlpha = imgOpacity * mask * hardThreshold( img, imgThresholds );

    // Compute segmentation alpha based on opacity and mask:
    float segAlpha = segInterpOpacity * segOpacity * getSegInteriorAlpha( seg ) * float(segMask);

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
