#include "logic/segmentation/Poisson.h"

#include "image/Image.h"

#include <spdlog/spdlog.h>

#include <cmath>

namespace
{

// Normalize image to rage [0.0, 1.0].
// void normalize( std::vector<ImgType>& img )
// {
//     ImgType maxVal = std::numeric_limits<ImgType>::lowest();
//     ImgType minVal = std::numeric_limits<ImgType>::max();

//     for ( const auto& val : img )
//     {
//         if ( val > maxVal )
//         {
//             maxVal = val;
//         }

//         if ( val < minVal )
//         {
//             minVal = val;
//         }
//     }

//     for ( size_t i = 0; i < img.size(); ++i )
//     {
//         img[i] = ( img[i] - minVal ) / ( maxVal - minVal );
//     }
// }

}


void sor(
    const Image& seedSeg, const Image& image, Image& potentialImage,
    uint32_t imComp, float rjac, uint32_t maxits, float beta )
{
    const uint8_t* seedData = static_cast<const uint8_t*>( seedSeg.bufferAsVoid(0) );
    const int16_t* imageData = static_cast<const int16_t*>( image.bufferAsVoid(imComp) );
    float* potData = static_cast<float*>( potentialImage.bufferAsVoid(0) );

    int n = 0;
    int isw, jsw, ksw;

    float omega = 1.0f;
    float val, pot, grad, weight;

    const glm::ivec3 dims = glm::ivec3{ image.header().pixelDimensions() };
    const glm::vec3 spacing = image.header().spacing();

    const int zDelta = dims.x * dims.y;
    const int yDelta = dims.x;
    const int xDelta = 1;

    for ( uint32_t iter = 0; iter < maxits; ++iter )
    {
        if ( 0 == iter % 1 )
        {
            spdlog::trace( "Iteration {}", iter );
        }

        float absResid = 0.0f;

        // Split updates into even and odd stencil passes:
        ksw = 0;

        for ( int pass = 0; pass < 2; ++pass )
        {
            jsw = ksw;

            for ( int k = 0; k < dims.z; ++k )
            {
                isw = jsw;

                for ( int j = 0; j < dims.y; ++j )
                {
                    for ( int i = isw; i < dims.x; i += 2 )
                    {
                        n = k * zDelta + j * yDelta + i;

                        if ( 0 != seedData[n] )
                        {
                            // Do not update nodes on boundary
                            potData[n] = seedData[n];
                            continue;
                        }

                        val = imageData[n];
                        pot = potData[n];

                        float resid = 0.0f;
                        float total = 0.0f;

                        if ( k < dims.z - 1 )
                        {
                            grad = (val - imageData[n + zDelta]) / beta;
                            weight = std::exp( -0.5f * grad * grad ) / spacing.z;
                            resid += weight * potData[n + zDelta];
                            total -= weight;
                        }

                        if ( k > 0 )
                        {
                            grad = (val - imageData[n - zDelta]) / beta;
                            weight = std::exp( -0.5f * grad * grad ) / spacing.z;
                            resid += weight * potData[n - zDelta];
                            total -= weight;
                        }

                        if ( j < dims.y - 1 )
                        {
                            grad = (val - imageData[n + yDelta]) / beta;
                            weight = std::exp( -0.5f * grad * grad ) / spacing.y;
                            resid += weight * potData[n + yDelta];
                            total -= weight;
                        }

                        if ( j > 0 )
                        {
                            grad = (val - imageData[n - yDelta]) / beta;
                            weight = std::exp( -0.5f * grad * grad ) / spacing.y;
                            resid += weight * potData[n - yDelta];
                            total -= weight;
                        }

                        if ( i < dims.x - 1 )
                        {
                            grad = (val - imageData[n + xDelta]) / beta;
                            weight = std::exp( -0.5f * grad * grad ) / spacing.x;
                            resid += weight * potData[n + xDelta];
                            total -= weight;
                        }

                        if ( i > 0 )
                        {
                            grad = (val - imageData[n - xDelta]) / beta;
                            weight = std::exp( -0.5f * grad * grad ) / spacing.x;
                            resid += weight * potData[n - xDelta];
                            total -= weight;
                        }

                        resid += total * pot;
                        potData[n] -= omega * resid / total;
                        absResid += std::fabs(resid);
                    }

                    isw = 1 - isw;
                }

                jsw = 1 - jsw;
            }

            ksw = 1 - ksw;

            omega =	( 0 == iter && 0 == pass )
                ? 1.0f / ( 1.0f - 0.5f * rjac * rjac )
                : 1.0f / ( 1.0f - 0.25f * rjac * rjac * omega );
        }

        spdlog::debug( "absResid = {}", absResid );
    }
}


float computeBeta( const Image& img, uint32_t imComp )
{
    const glm::ivec3 dims = glm::ivec3{ img.header().pixelDimensions() };

    float grad = 0.0f;

    for ( int k = 0; k < dims.z; ++k ) {
        for ( int j = 0; j < dims.y; ++j ) {
            for ( int i = 0; i < dims.x; ++i )
            {
                const float val = img.value<float>(imComp, i, j, k).value_or(0.0f);

                if (k < dims[2] - 1) {
                    grad += std::fabs( val - img.value<float>(imComp, i, j, k + 1).value_or(0.0f) );
                }

                if (k > 0) {
                    grad += std::fabs( val - img.value<float>(imComp, i, j, k - 1).value_or(0.0f) );
                }

                if (j < dims.y - 1) {
                    grad += std::fabs( val - img.value<float>(imComp, i, j + 1, k).value_or(0.0f) );
                }

                if (j > 0) {
                    grad += std::fabs( val - img.value<float>(imComp, i, j - 1, k).value_or(0.0f) );
                }

                if (i < dims.x - 1) {
                    grad += std::fabs( val - img.value<float>(imComp, i + 1, j, k).value_or(0.0f) );
                }

                if (i > 0) {
                    grad += std::fabs( val - img.value<float>(imComp, i - 1, j, k).value_or(0.0f) );
                }
            }
        }
    }

    return grad / static_cast<float>( dims.x * dims.y * dims.z );
}


//input initial approximation to  potential (double format)
//output potential (double format)
//potential boundary values in (int16_t format)
//maximum number of SOR iterations
//(optional): input conductivity image in (double format)
