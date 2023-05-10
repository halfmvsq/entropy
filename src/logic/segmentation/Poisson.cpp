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
    const Image& bnd, const Image& img, Image& pot,
    uint32_t imComp,
    float rjac, uint32_t maxits, float beta )
{
    int isw, jsw, ksw;

    double omega = 1.0;
    double grad, weight;

    const glm::ivec3 dims = glm::ivec3{ img.header().pixelDimensions() };
    const glm::vec3 spacing = img.header().spacing();

    for ( uint32_t iter = 0; iter < maxits; ++iter )
    {
        if ( 0 == iter % 100 )
        {
            spdlog::trace( "Iteration {}", iter );
        }

        double absResid = 0.0;

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
                        // Do not update nodes on boundary
                        if ( 0 != bnd.value<int64_t>(0, i, j, k).value_or(0) )
                        {
                            continue;
                        }

                        const float val = img.value<float>(imComp, i, j, k).value_or(0.0f);

                        float resid = 0.0f;
                        float total = 0.0f;

                        if ( k < dims.z - 1 )
                        {
                            grad = (val - img.value<float>(imComp, i, j, k + 1).value_or(0.0f)) / beta;
                            weight = std::exp( -grad*grad / 2.0f ) / spacing.z;
                            resid += weight * pot.value<float>(0, i, j, k + 1).value_or(0.0f);
                            total -= weight;
                        }

                        if ( k > 0 )
                        {
                            grad = (val - img.value<float>(imComp, i, j, k - 1).value_or(0.0f)) / beta;
                            weight = std::exp( -grad*grad / 2.0f ) / spacing.z;
                            resid += weight * pot.value<float>(0, i, j, k - 1).value_or(0.0f);
                            total -= weight;
                        }

                        if ( j < dims.y - 1 )
                        {
                            grad = (val - img.value<float>(imComp, i, j + 1, k).value_or(0.0f)) / beta;
                            weight = std::exp( -grad*grad / 2.0f ) / spacing.y;
                            resid += weight * pot.value<float>(0, i, j + 1, k).value_or(0.0f);
                            total -= weight;
                        }

                        if ( j > 0 )
                        {
                            grad = (val - img.value<float>(imComp, i, j - 1, k).value_or(0.0f)) / beta;
                            weight = std::exp( -grad*grad / 2.0f ) / spacing.y;
                            resid += weight * pot.value<float>(0, i, j - 1, k).value_or(0.0f);
                            total -= weight;
                        }

                        if ( i < dims.x - 1 )
                        {
                            grad = (val - img.value<float>(imComp, i + 1, j, k).value_or(0.0f)) / beta;
                            weight = std::exp( -grad*grad/2.0f ) / spacing.x;
                            resid += weight * pot.value<float>(0, i + 1, j, k).value_or(0.0f);
                            total -= weight;
                        }

                        if ( i > 0 )
                        {
                            grad = (val - img.value<float>(imComp, i - 1, j, k).value_or(0.0f)) / beta;
                            weight = std::exp( -grad*grad / 2.0 ) / spacing.x;
                            resid += weight * pot.value<float>(0, i - 1, j, k).value_or(0.0f);
                            total -= weight;
                        }

                        const float p = pot.value<float>(0, i, j, k).value_or(0.0f);

                        resid += total * p;
                        pot.setValue(0, i, j, k, p - omega * resid / total );

                        absResid += std::fabs(resid);
                    }

                    isw = 1 - isw;
                }

                jsw = 1 - jsw;
            }

            ksw = 1 - ksw;

            omega =	(iter == 0 && pass == 0 )
                ? 1.0 / ( 1.0 - 0.5 * rjac * rjac )
                : 1.0 / ( 1.0 - 0.25 * rjac * rjac * omega );
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
