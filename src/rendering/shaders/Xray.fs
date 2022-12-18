#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3( 0.0 )
#define MAX_IMAGE_TEXCOORD vec3( 1.0 )

// Rendering modes:
#define IMAGE_RENDER_MODE      0
#define CHECKER_RENDER_MODE    1
#define QUADRANTS_RENDER_MODE  2
#define FLASHLIGHT_RENDER_MODE 3


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

uniform bool useTricubicInterpolation; // Whether to use tricubic interpolation

// Slope for mapping texture intensity to native image intensity, NOT accounting for window-leveling
uniform float imgSlope_native_T_texture;

uniform vec2 imgMinMax; // Min and max image values

// Low and high image thresholds, expressed in image texture values
uniform vec2 imgThresholds;

// Slope and intercept for window-leveling the attenuation value
uniform vec2 slopeInterceptWindowLevel;

uniform vec2 imgCmapSlopeIntercept; // Slopes and intercepts for the image color maps

uniform float imgOpacity; // Image opacities
uniform float segOpacity; // Segmentation opacities

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float segInteriorOpacity;

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

// Half the number of samples for MIP. Set to 0 when no projection is used.
uniform int halfNumMipSamples;

// Sampling distance in centimeters (used for X-ray IP mode)
uniform float mipSamplingDistance_cm;

// Z view camera direction, represented in texture sampling space
uniform vec3 texSamplingDirZ;

// Photon mass attenuation coefficients [1/cm] of liquid water and dry air (at sea level):
uniform float waterAttenCoeff;
uniform float airAttenCoeff;


// Check if inside texture coordinates
bool isInsideTexture( vec3 a )
{
    return ( all( greaterThanEqual( a, MIN_IMAGE_TEXCOORD ) ) &&
             all( lessThanEqual( a, MAX_IMAGE_TEXCOORD ) ) );
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


float getImageValue( vec3 texCoord )
{
    return clamp( mix(
        texture( imgTex, texCoord )[0],
        interpolateTricubicFast( imgTex, texCoord ),
        float(useTricubicInterpolation) ), imgMinMax[0], imgMinMax[1] );
}


// Convert texture intensity to Hounsefield Units, then to Photon Mass Attenuation coefficient
float convertTexToAtten( float texValue )
{
    // Hounsefield units:
    float hu = imgSlope_native_T_texture * texValue;

    // Photon mass attenuation coefficient:
    return max( ( hu / 1000.0 ) * ( waterAttenCoeff - airAttenCoeff ) + waterAttenCoeff, 0.0 );
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


float hardThreshold( float value, vec2 thresholds )
{
    return float( thresholds[0] <= value && value <= thresholds[1] );
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


void main()
{
    if ( ! doRender() ) discard;

    // Image and segmentation masks based on texture coordinates:
    bool imgMask = isInsideTexture( fs_in.ImgTexCoords );
    bool segMask = isInsideTexture( fs_in.SegTexCoords );

    // Look up the texture values and convert to mass attenuation coefficient.
    // Keep a running sum of attenuation for all samples.
    float texValue = getImageValue( fs_in.ImgTexCoords );
    float thresh = hardThreshold( texValue, imgThresholds );
    float totalAtten = thresh * convertTexToAtten( texValue );

    // Number of samples used for computing the final image value:
    int numSamples = int( thresh );

    // Accumulate intensity projection in forwards (+Z) direction:
    for ( int i = 1; i <= halfNumMipSamples; ++i )
    {
        vec3 tc = fs_in.ImgTexCoords + i * texSamplingDirZ;
        if ( ! isInsideTexture( tc ) ) break;

        texValue = getImageValue( tc );
        thresh = hardThreshold( texValue, imgThresholds );
        totalAtten += thresh * convertTexToAtten( texValue );
        numSamples += int( thresh );
    }

    // Accumulate intensity projection in backwards (-Z) direction:
    for ( int i = 1; i <= halfNumMipSamples; ++i )
    {
        vec3 tc = fs_in.ImgTexCoords - i * texSamplingDirZ;
        if ( ! isInsideTexture( tc ) ) break;

        texValue = getImageValue( tc );
        thresh = hardThreshold( texValue, imgThresholds );
        totalAtten += thresh * convertTexToAtten( texValue );
        numSamples += int( thresh );
    }

    // Compute inverse of the total photon attenuation, which is in range [0.0, 1):
    float invAtten = 1.0 - exp( -totalAtten * mipSamplingDistance_cm );

    // Apply window-leveling:
    float invAttenWL = slopeInterceptWindowLevel[0] * invAtten + slopeInterceptWindowLevel[1];

    // Look up segmentation texture label value:
    uint seg = texture( segTex, fs_in.SegTexCoords ).r;

    // Compute the image mask:
    float mask = float( imgMask && ( masking && ( seg > 0u ) || ! masking ) );

    // Compute image alpha based on opacity, mask, and thresholds:
    float imgAlpha = imgOpacity * mask;

    // Compute segmentation alpha based on opacity and mask:
    float segAlpha = getSegInteriorAlpha( seg ) * segOpacity * float(segMask);

    // Look up image color and apply alpha:
    vec4 imgColor = texture( imgCmapTex, imgCmapSlopeIntercept[0] * invAttenWL + imgCmapSlopeIntercept[1] ) * imgAlpha;

    // Look up segmentation color and apply:
    vec4 segColor = texelFetch( segLabelCmapTex, int(seg), 0 ) * segAlpha;

    // Blend colors:
    OutColor = vec4( 0.0, 0.0, 0.0, fs_in.SegVoxCoords.x * 0.000000001 );
    OutColor = imgColor + ( 1.0 - imgColor.a ) * OutColor;
    OutColor = segColor + ( 1.0 - segColor.a ) * OutColor;
}
