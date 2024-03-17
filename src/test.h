#pragma once

#include "image/ImageUtility.tpp"

#include <glm/glm.hpp>
#include <itkImage.h>
#include <itkVector.h>
#include <array>

void test()
{
    uint32_t Ni = 128;
    uint32_t Nj = 128;
    uint32_t Nk = 128;

    glm::vec3 center(Ni/2, Nj/2, Nk/2);

    const std::array< std::array<double, 3>, 3 > dirs
        {
            std::array<double, 3>{1,0,0},
            std::array<double, 3>{0,1,0},
            std::array<double, 3>{0,0,1}
        };

    std::vector<float> im(Ni*Nj*Nk, 0.0f);

    for (uint32_t k = 0; k < Nk; ++k) {
        for (uint32_t j = 0; j < Nj; ++j) {
            for (uint32_t i = 0; i < Ni; ++i)
            {
                glm::vec3 p(i,j,k);
                im[i + j*Ni + k*Ni*Nj] = glm::distance(p, center);
            }
        }
    }

    const itk::Image<float, 3>::Pointer image = makeScalarImage(
        {Ni, Nj, Nk}, {0,0,0}, {1,1,1}, dirs, im.data());

    writeImage<float, 3, false>(image, "/Users/danadler/im.nrrd");


    std::vector<float> def(Ni*Nj*Nk*3, 0.0f);

    for (uint32_t k = 0; k < Nk; ++k) {
        for (uint32_t j = 0; j < Nj; ++j) {
            for (uint32_t i = 0; i < Ni; ++i)
            {
                uint32_t index = 3 * (i + j*Ni + k*Ni*Nj);
                glm::vec3 p(i,j,k);
                glm::vec3 d = glm::normalize(p - center);
                def[index + 0] = d.x;
                def[index + 1] = d.y;
                def[index + 2] = d.z;
            }
        }
    }

    typename itk::Image<itk::Vector<float, 3>, 3>::Pointer field = makeVectorImage<float, 3>(
        {Ni, Nj, Nk}, {0,0,0}, {1,1,1}, dirs, def.data() );

    writeImage<itk::Vector<float, 3>, 3, false>(field, "/Users/danadler/def.nii.gz");
}
