#pragma once

#include <array>
#include <cstdint>
#include <optional>

/**
 * @brief Method used to compute number of histogram bins in a scalar (single component) image
 * @see https://numpy.org/doc/stable/reference/generated/numpy.histogram_bin_edges.html
 * @see https://en.wikipedia.org/wiki/Histogram#Number_of_bins_and_width
 */
enum class NumBinsComputationMethod
{
    SquareRoot,
    Sturges,
    Rice,
    Scott,
    FreedmanDiaconis,
    UserDefined //!< User defined number
};

/**
 * @brief Settings used for computing and displaying the histogram of a single image component
 */
struct HistogramSettings
{
    NumBinsComputationMethod m_numBinsMethod;
    int m_numBins; //!< Number of bins in histogram

    bool m_isCumulative; //!< Whether the histogram is cumulative (\c ImPlotHistogramFlags_Cumulative)
    bool m_isDensity; //!< Whehter the histogram shows probability densities (true) or counts (false) (\c ImPlotHistogramFlags_Density)
    bool m_isHorizontal; //!< Whether the histogram is horizontal, i.e. bins go left to right (\c ImPlotHistogramFlags_Horizontal)
    bool m_isLogScale; //!< Apply log (base 10) to histogram bin counts/probabilities

    /// Intensity range (inclusive) over which to compute histogram.
    /// If not defined, then the image component's [min, max] range will be used
    std::array<float, 2> m_intensityRange;
    bool m_useCustomIntensityRange; //!< Whether to use the custom intensity range
};
