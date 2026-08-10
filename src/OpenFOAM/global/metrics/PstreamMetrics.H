/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | www.openfoam.com
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

Namespace
    Foam::pstreamMetrics

Description
    Non-throwing OpenFOAM adapter for the optional Pstream metrics hook.

\*---------------------------------------------------------------------------*/

#ifndef Foam_PstreamMetrics_H
#define Foam_PstreamMetrics_H

#include <optional>

namespace Foam
{
namespace pstreamMetrics
{

void startupApplied
(
    int configuredMpiBufferValue,
    const std::optional<int>& environmentMpiBufferBytesApplied,
    int attachedBufferBytes
) noexcept;

void runtimeCurrent() noexcept;

void runtimeChanged() noexcept;

} // End namespace pstreamMetrics
} // End namespace Foam

#endif

// ************************************************************************* //
