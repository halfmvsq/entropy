#ifndef POISSON_H
#define POISSON_H

#include <array>
#include <vector>

using BndType = short;
using ImgType = float;
using PotType = float;

/**
 * @brief sor
 * @param bnd
 * @param img
 * @param pot
 * @param dims
 * @param spacing
 * @param rjac
 * @param maxits
 * @param beta
 *
 * @cite This code is a 3D extension of the algorithm from "Numerical Recipes in C":
 * "Successive over-relaxation solution of equation (19.5.25) with Chebyshev acceleration"
 * 'rjac' is input as the spectral radius of the Jacobi iteration, or an estimate of it.
 */
void sor( const std::vector<BndType>& bnd,
         const std::vector<ImgType>& img,
         std::vector<PotType>& pot,
         const std::array<int, 3>& dims,
         const std::array<double, 3>& spacing,
         double rjac,
         std::size_t maxits,
         double beta );

#endif // POISSON_H
