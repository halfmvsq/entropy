#version 330 core

#define MIN_IMAGE_TEXCOORD vec3( 0.0 )
#define MAX_IMAGE_TEXCOORD vec3( 1.0 )

#define N 2

in VS_OUT // Redeclared vertex shader outputs: now the fragment shader inputs
{
    vec3 ImgTexCoords[N];
    vec3 SegTexCoords[N];
} fs_in;

layout (location = 0) out vec4 OutColor; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D imgTex[N]; // Texture units 0/1: images
uniform usampler3D segTex[N]; // Texture units 2/3: segmentations
uniform sampler1D segLabelCmapTex[N]; // Texutre unit 4/5: label color maps (pre-mult RGBA)

uniform bool useTricubicInterpolation; // Whether to use tricubic interpolation

uniform vec2 imgSlopeIntercept[N]; // Slopes and intercepts for image window-leveling

uniform vec2 imgMinMax[N]; // Min and max image values
uniform vec2 imgThresholds[N]; // Image lower and upper thresholds, mapped to OpenGL texture intensity
uniform float imgOpacity[N]; // Image opacities
uniform float segOpacity[N]; // Segmentation opacities

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float segInteriorOpacity;

uniform bool magentaCyan;


float smoothThreshold( float value, vec2 thresholds )
{
    return smoothstep( thresholds[0] - 0.01, thresholds[0], value ) -
           smoothstep( thresholds[1], thresholds[1] + 0.01, value );
}

float hardThreshold( float value, vec2 thresholds )
{
    return float( thresholds[0] <= value && value <= thresholds[1] );
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
float getSegInteriorAlpha( int texNum, uint seg )
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
        uint nseg = texture( segTex[texNum], fs_in.SegTexCoords[texNum] + texSamplingPos )[0];

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


float getImageValue( sampler3D tex, vec3 texCoord, vec2 minMax )
{
    return clamp( mix(
        texture( tex, texCoord )[0],
        interpolateTricubicFast( tex, texCoord ),
        float(useTricubicInterpolation) ), minMax[0], minMax[1] );
}


void main()
{
    vec4 overlapColor = vec4( 0.0, 0.0, 0.0, 0.0 );
    vec4 segColor[2];

    for ( int i = 0; i < N; ++i )
    {
        // Foreground masks, based on whether texture coordinates are in range [0.0, 1.0]^3:
        bool imgMask = ! ( any(    lessThan( fs_in.ImgTexCoords[i], MIN_IMAGE_TEXCOORD ) ) ||
                           any( greaterThan( fs_in.ImgTexCoords[i], MAX_IMAGE_TEXCOORD ) ) );

        bool segMask = ! ( any(    lessThan( fs_in.SegTexCoords[i], MIN_IMAGE_TEXCOORD ) ) ||
                           any( greaterThan( fs_in.SegTexCoords[i], MAX_IMAGE_TEXCOORD ) ) );

        // float val = texture( imgTex[i], fs_in.ImgTexCoords[i] ).r; // Image value
        float val = getImageValue( imgTex[i], fs_in.ImgTexCoords[i], imgMinMax[i] );

        uint label = texture( segTex[i], fs_in.SegTexCoords[i] ).r; // Label value
        float norm = clamp( imgSlopeIntercept[i][0] * val + imgSlopeIntercept[i][1], 0.0, 1.0 ); // Apply W/L

        // Apply opacity, foreground mask, and thresholds for images:
        float alpha = imgOpacity[i] * float(imgMask) * hardThreshold( val, imgThresholds[i] );

        // Look up label colors:
        segColor[i] = texelFetch( segLabelCmapTex[i], int(label), 0 ) * getSegInteriorAlpha( i, label ) * segOpacity[i] * float(segMask);

        overlapColor[i] = norm * alpha;
    }

    // Apply magenta/cyan option:
    overlapColor.b = float( magentaCyan ) * max( overlapColor.r, overlapColor.g );

    // Turn on overlap color if either the reference or moving image is present:
    // (note that this operation effectively thresholds out 0.0 as background)
    overlapColor.a = float( ( overlapColor.r > 0.0 ) || ( overlapColor.g > 0.0 ) );
    overlapColor.rgb = overlapColor.a * overlapColor.rgb;

    OutColor = segColor[0] + ( 1.0 - segColor[0].a ) * overlapColor;
    OutColor = segColor[1] + ( 1.0 - segColor[1].a ) * OutColor;
}
