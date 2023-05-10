#ifndef POISSON_H
#define POISSON_H

#include <cstdint>

class Image;

/**
 * @cite This code is a 3D extension of the algorithm from "Numerical Recipes in C":
 * "Successive over-relaxation solution of equation (19.5.25) with Chebyshev acceleration"
 * 'rjac' is input as the spectral radius of the Jacobi iteration, or an estimate of it.
 */
void sor(
    const Image& bnd, const Image& img, Image& pot,
    uint32_t imComp, float rjac, uint32_t maxits, float beta );

// Compute a decent value for the 'beta' parameter used in SOR.
float computeBeta( const Image& img, uint32_t imComp );

#endif // POISSON_H
