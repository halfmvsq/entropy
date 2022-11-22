#include "ui/GuiData.h"

void GuiData::setCoordsPrecisionFormat()
{
    m_coordsPrecisionFormat = std::string( "%0." ) +
            std::to_string( m_coordsPrecision ) + "f";
}

void GuiData::setTxPrecisionFormat()
{
    m_txPrecisionFormat = std::string( "%0." ) +
            std::to_string( m_coordsPrecision ) + "f";
}
