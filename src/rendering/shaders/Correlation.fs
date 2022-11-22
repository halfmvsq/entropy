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
uniform sampler1D segLabelCmapTex[N]; // Texutre unit 6/7: label color tables (pre-mult RGBA)

uniform float segOpacity[N]; // Segmentation opacities

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float segInteriorOpacity;

uniform sampler1D metricCmapTex; // Texture unit 4: metric colormap (pre-mult RGBA)
uniform vec2 metricCmapSlopeIntercept; // Slope and intercept for the metric colormap
uniform vec2 metricSlopeIntercept; // Slope and intercept for the final metric
uniform bool metricMasking; // Whether to mask based on segmentation

uniform mat4 texture1_T_texture0;
uniform vec3 texSampleSize[N];
uniform vec3 tex0SamplingDirX;
uniform vec3 tex0SamplingDirY;


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
        float col = float( floor( i / 3 ) - 1 ); // runs -1 to 1

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


void main()
{
    float imgNorm[N];
    float mask[N];
    vec4 segColor[N];

    for ( int i = 0; i < N; ++i )
    {
        // Foreground masks, based on whether texture coordinates are in range [0.0, 1.0]^3:
        bool imgMask = ! ( any(    lessThan( fs_in.ImgTexCoords[i], MIN_IMAGE_TEXCOORD + texSampleSize[i] ) ) ||
                           any( greaterThan( fs_in.ImgTexCoords[i], MAX_IMAGE_TEXCOORD - texSampleSize[i] ) ) );

        bool segMask = ! ( any(    lessThan( fs_in.SegTexCoords[i], MIN_IMAGE_TEXCOORD + texSampleSize[i] ) ) ||
                           any( greaterThan( fs_in.SegTexCoords[i], MAX_IMAGE_TEXCOORD - texSampleSize[i] ) ) );

        uint label = texture( segTex[i], fs_in.SegTexCoords[i] ).r; // Label value

        // Apply foreground mask and masking based on segmentation label:
        mask[i] = float( imgMask && ( metricMasking && ( label > 0u ) || ! metricMasking ) );

        // Look up label colors:
        segColor[i] = texelFetch( segLabelCmapTex[i], int(label), 0 ) * getSegInteriorAlpha( i, label ) * segOpacity[i] * float(segMask);
    }

    float val0[9];
    float val1[9];

    int count = 0;
    for ( int j = -1; j <= 1; ++j )
    {
        for ( int i = -1; i <= 1; ++i )
        {
            vec3 tex0Pos = fs_in.ImgTexCoords[0] + float(i) * tex0SamplingDirX + float(j) * tex0SamplingDirY;
            vec3 tex1Pos = vec3( texture1_T_texture0 * vec4( tex0Pos, 1.0 ) );

            val0[count] = texture( imgTex[0], tex0Pos ).r;
            val1[count] = texture( imgTex[1], tex1Pos ).r;
            ++count;
        }
    }

    float mean0 = 0.0;
    float mean1 = 0.0;

    for ( int i = 0; i < 9; ++i )
    {
        mean0 += val0[i] / 9.0;
        mean1 += val1[i] / 9.0;
    }

    float numer = 0.0;
    float denom0 = 0.0;
    float denom1 = 0.0;

    for ( int i = 0; i < 9; ++i )
    {
        float a = ( val0[i] - mean0 );
        float b = ( val1[i] - mean1 );
        numer += a * b;
        denom0 += a * a;
        denom1 += b * b;
    }

    // Overall mask is the AND of both image masks:
    float metricMask = mask[0] * mask[1];

    // Compute metric and apply window/level to it:
    //float metric = ( val0 * val1 ) * metricMask;
    float metric = numer / sqrt( denom0 * denom1 );
    metric = 0.5 * ( metric + 1.0 );

    //metric = clamp( metricSlopeIntercept[0] * metric + metricSlopeIntercept[1], 0.0, 1.0 );
    metric = metricSlopeIntercept[0] * metric + metricSlopeIntercept[1];

    // Index into colormap:
    float cmapValue = metricCmapSlopeIntercept[0] * metric + metricCmapSlopeIntercept[1];

    // Apply colormap and masking (by pre-multiplying RGBA with alpha mask):
    vec4 metricColor = texture( metricCmapTex, cmapValue ) * metricMask;

    // Ignore the segmentation layers if metric masking is enabled:
    float overlaySegs = float( ! metricMasking );
    segColor[0] = overlaySegs * segColor[0];
    segColor[1] = overlaySegs * segColor[1];

    // Blend colors:
    OutColor = vec4( 0.0, 0.0, 0.0, 0.0 );
    OutColor = segColor[0] + ( 1.0 - segColor[0].a ) * metricColor;
    OutColor = segColor[1] + ( 1.0 - segColor[1].a ) * OutColor;
}
