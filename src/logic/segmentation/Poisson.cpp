#include "logic/segmentation/Poisson.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>


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


void sor( const std::vector<BndType>& bnd,
         const std::vector<ImgType>& img,
         std::vector<PotType>& pot,
         const std::array<int, 3>& dims,
         const std::array<double, 3>& spacing,
         double rjac,
         size_t maxits,
         double beta )
{
    size_t n = 0;
    size_t p = 0;

    int isw, jsw, ksw;

    double omega = 1.0;
    double val, grad, weight;

    for ( size_t iter = 0; iter < maxits; ++iter )
    {
        if ( 0 == iter % 100 )
        {
            std::cout << "\n\niter " << iter << ": " << std::flush;
        }

        double absResid = 0.0;

        // Split updates into even and odd stencil passes:
        ksw = 0;

        for ( int pass = 0; pass < 2; ++pass )
        {
            jsw = ksw;

            for ( int k = 0; k < dims[2]; ++k )
            {
                isw = jsw;

                for ( int j = 0; j < dims[1]; ++j )
                {
                    for ( int i = isw; i < dims[0]; i += 2 )
                    {
                        n = (k*dims[1] + j)*dims[0] + i;

                        // Do not update nodes on boundary
                        if ( 0 != bnd[n] )
                        {
                            continue;
                        }

                        val = img[n];

                        double resid = 0.0;
                        double total = 0.0;

                        if ( k + 1 <= dims[2] - 1 )
                        {
                            p = ((k+1)*dims[1] + j)*dims[0] + i;
                            grad = (val - img[p]) / beta;
                            weight = std::exp(-grad*grad/2.0) / spacing[2];
                            resid += weight * pot[p];
                            total -= weight;
                        }

                        if ( k  -1 >= 0 )
                        {
                            p = ((k-1)*dims[1] + j)*dims[0] + i;
                            grad = (val - img[p]) / beta;
                            weight = std::exp(-grad*grad/2.0) / spacing[2];
                            resid += weight * pot[p];
                            total -= weight;
                        }

                        if ( j + 1 <= dims[1] - 1 )
                        {
                            p = (k*dims[1] + (j+1))*dims[0] + i;
                            grad = (val - img[p]) / beta;
                            weight = std::exp(-grad*grad/2.0) / spacing[1];
                            resid += weight * pot[p];
                            total -= weight;
                        }

                        if ( j - 1 >= 0 )
                        {
                            p = (k*dims[1] + (j-1))*dims[0] + i;
                            grad = (val - img[p]) / beta;
                            weight = std::exp(-grad*grad/2.0) / spacing[1];
                            resid += weight * pot[p];
                            total -= weight;
                        }

                        if ( i + 1 <= dims[0] - 1 )
                        {
                            p = (k*dims[1] + j)*dims[0] + (i+1);
                            grad = (val - img[p]) / beta;
                            weight = std::exp(-grad*grad/2.0) / spacing[0];
                            resid += weight * pot[p];
                            total -= weight;
                        }

                        if ( i - 1 >= 0 )
                        {
                            p = (k*dims[1] + j)*dims[0] + (i-1);
                            grad = (val - img[p]) / beta;
                            weight = std::exp(-grad*grad/2.0) / spacing[0];
                            resid += weight * pot[p];
                            total -= weight;
                        }


                        resid += total * pot[n];

                        absResid += std::fabs(resid);
                        pot[n] -= omega * resid / total;
                    }

                    isw = 1 - isw;
                }

                jsw = 1 - jsw;
            }

            ksw = 1 - ksw;

            // These update formulae are designed for the 2D case and do not apply
            // exactly in the 3D case
            omega =	(iter == 0 && pass == 0 )
                        ? 1.0 / ( 1.0 - 0.5 * rjac * rjac )
                        : 1.0 / ( 1.0 - 0.25 * rjac * rjac * omega );
        }

        std::cout << absResid << " " << std::flush;
    }

    std::cout << std::endl;
}


// Compute a decent value for the 'beta' parameter used in SOR.
double computeBeta(
    const std::vector<ImgType>& img,
    const std::array<int, 3>& dims )
{
    ImgType val = 0.0;
    ImgType grad = 0.0;

    for ( int k = 0; k < dims[2]; ++k )
    {
        for ( int j = 0; j < dims[1]; ++j )
        {
            for ( int i = 0; i < dims[0]; ++i )
            {
                const size_t n = (k*dims[1] + j)*dims[0] + i;
                val = img[n];

                if (k+1 <= dims[2]-1)
                {
                    size_t p = ((k+1)*dims[1] + j)*dims[0] + i;
                    grad += std::fabs((val - img[p]));
                }

                if (k-1 >= 0)
                {
                    size_t p = ((k-1)*dims[1] + j)*dims[0] + i;
                    grad += std::fabs((val - img[p]));
                }

                if (j+1 <= dims[1]-1)
                {
                    size_t p = (k*dims[1] + (j+1))*dims[0] + i;
                    grad += std::fabs((val - img[p]));
                }

                if (j-1 >= 0)
                {
                    size_t p = (k*dims[1] + (j-1))*dims[0] + i;
                    grad += std::fabs((val - img[p]));
                }

                if (i+1 <= dims[0]-1)
                {
                    size_t p = (k*dims[1] + j)*dims[0] + (i+1);
                    grad += std::fabs((val - img[p]));
                }

                if (i-1 >= 0)
                {
                    size_t p = (k*dims[1] + j)*dims[0] + (i-1);
                    grad += std::fabs((val - img[p]));
                }
            }
        }
    }

    return grad / static_cast<ImgType>( dims[0] * dims[1] * dims[2] );
}



// Input parameters:
// 0: poisson
// 1: potential boundary values in (int16_t format)
// 2: input initial approximation to  potential (double format)
// 3: output potential (double format)
// 4: x dimensions of all images
// 5: y dimensions of all images
// 6: z dimensions of all images
// 7: x voxel spacing of all images
// 8: y voxel spacing of all images
// 9: z voxel spacing of all images
// 10: maximum number of SOR iterations
// 11: rjac
// 12 (optional): input conductivity image in (double format)

#if 0
int main( int argc, char** argv )
{
    if ( argc < 12 )
    {
        return 1;
    }

    const std::string bndPath( argv[1] ); // fixed potential boundary values (input)
    const std::string inputPotPath( argv[2] ); // initial scalar potential (input)
    const std::string outPotPath( argv[3] ); // output scalar potential (output)

    // image dimensions (x, y, z)
    const std::array<int, 3> dims{
                                  std::atoi( argv[4] ),
                                  std::atoi( argv[5] ),
                                  std::atoi( argv[6] ) };

    // image voxel spacing (x, y, z)
    const std::array<double, 3> spacing{
                                        std::atof( argv[7] ),
                                        std::atof( argv[8] ),
                                        std::atof( argv[9] ) };

    const size_t maxits = atoi( argv[10] );
    const double rjac = atof( argv[11] );
    double beta = 1.0;

    const size_t N = dims[0] * dims[1] * dims[2];

    std::vector<BndType> bnd( N ); // potential boundary conditions
    std::vector<ImgType> img( N, 0.0 ); // image (default to zero)
    std::vector<ImgType> pot( N ); // potential

    // Load boundary conditions:
    std::ifstream ifs( bndPath, std::ifstream::in | std::ifstream::binary );

    if ( ifs.is_open() )
    {
        ifs.read( (char*) bnd.data(), N * sizeof(BndType) );
        ifs.close();
    }
    else
    {
        std::cout << "Unable to open input boundary condition file" << std::endl;
        ifs.close();
        return 1;
    }

    // Load initial potential:
    ifs.open( inputPotPath, std::ifstream::in | std::ifstream::binary) ;

    if ( ifs.is_open() )
    {
        ifs.read( (char*) pot.data(), N * sizeof(PotType) );
        ifs.close();
    }
    else
    {
        std::cout << "Unable to open input initial potential file" << std::endl;
        ifs.close();
        return 1;
    }

    if ( 13 == argc )
    {
        // Image provided for 'electrical conductivity':
        const std::string imgPath( argv[12] );

        ifs.open( imgPath, std::ifstream::in | std::ifstream::binary );

        if ( ifs.is_open() )
        {
            ifs.read( (char*) img.data(), N * sizeof(ImgType) );
            ifs.close();
        }
        else
        {
            std::cout << "Unable to open input image" << std::endl;
            ifs.close();
            return 1;
        }

        normalize( img );

        beta = computeBeta( img, dims );
        std::cout << std::endl << "beta = " << beta << std::endl;
    }


    sor( bnd, img, pot, dims, spacing, rjac, maxits, beta );

    std::ofstream ofs( outPotPath, std::ios::out | std::ios::binary );

    if ( ofs.is_open() )
    {
        ofs.write( (char*) pot.data(), N * sizeof(PotType) );
    }
    else
    {
        std::cout << "Unable to open output potential file" << std::endl;
        return 1;
    }

    ofs.close();

    return 0;
}
#endif
