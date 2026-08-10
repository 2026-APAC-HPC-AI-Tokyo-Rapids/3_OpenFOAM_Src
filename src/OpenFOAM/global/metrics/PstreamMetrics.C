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

\*---------------------------------------------------------------------------*/

#include "PstreamMetrics.H"
#include "PstreamMetricsWriter.H"
#include "PstreamBuffers.H"
#include "UPstream.H"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace
{

std::unique_ptr<Foam::PstreamMetricsWriter>& writer()
{
    static std::unique_ptr<Foam::PstreamMetricsWriter> instance;
    return instance;
}


Foam::PstreamMetricsWriter::RuntimeState runtimeState()
{
    using Foam::PstreamBuffers;
    using Foam::UPstream;

    std::string commsType;
    switch (UPstream::defaultCommsType)
    {
        case UPstream::commsTypes::buffered:
            commsType = "buffered";
            break;
        case UPstream::commsTypes::scheduled:
            commsType = "scheduled";
            break;
        case UPstream::commsTypes::nonBlocking:
            commsType = "nonBlocking";
            break;
    }

    return
    {
        UPstream::floatTransfer,
        UPstream::nProcsSimpleSum,
        UPstream::nProcsNonblockingExchange,
        UPstream::tuning_NBX_,
        PstreamBuffers::algorithm,
        std::move(commsType),
        UPstream::nPollProcInterfaces,
        UPstream::maxCommsSize,
        UPstream::nodeCommsControl_,
        UPstream::nodeCommsMin_,
        UPstream::topologyControl_,
        UPstream::usingNodeComms(UPstream::worldComm)
    };
}

} // End anonymous namespace


void Foam::pstreamMetrics::startupApplied
(
    const int configuredMpiBufferValue,
    const std::optional<int>& environmentMpiBufferBytesApplied,
    const int attachedBufferBytes
) noexcept
{
    try
    {
        // The sink is supervisor-owned. Never create it or infer identity
        // from its path.
        const char* directory = std::getenv("OPENFOAM_METRICS_PSTREAM_DIR");
        if (!directory || !*directory)
        {
            return;
        }

        const char* runId = std::getenv("OPENFOAM_METRICS_RUN_ID");
        if (!runId || !*runId || writer())
        {
            return;
        }

        // The filename/envelope rank is the global launcher rank, including
        // when OpenFOAM local worlds are in use.
        const int processRank = UPstream::myProcNo(UPstream::commGlobal());
        if (processRank < 0)
        {
            return;
        }

        writer() = std::make_unique<PstreamMetricsWriter>
        (
            directory,
            runId,
            static_cast<unsigned>(processRank)
        );

        writer()->writeStartup
        ({
            UPstream::nodeCommsControl_,
            UPstream::nodeCommsMin_,
            UPstream::usingNodeComms(UPstream::worldComm),
            UPstream::numNodes(),
            UPstream::topologyControl_,
            configuredMpiBufferValue,
            environmentMpiBufferBytesApplied,
            attachedBufferBytes
        });
    }
    catch (...)
    {
        writer().reset();
    }
}


void Foam::pstreamMetrics::runtimeCurrent() noexcept
{
    try
    {
        if (writer())
        {
            writer()->writeRuntime(runtimeState());
        }
    }
    catch (...)
    {
        writer().reset();
    }
}


void Foam::pstreamMetrics::runtimeChanged() noexcept
{
    try
    {
        if (writer())
        {
            writer()->writeChanges(runtimeState());
        }
    }
    catch (...)
    {
        writer().reset();
    }
}

// ************************************************************************* //
