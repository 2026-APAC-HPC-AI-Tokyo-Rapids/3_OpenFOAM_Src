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

Class
    Foam::PstreamMetricsWriter

Description
    Best-effort line-buffered writer for openfoam-pstream-event/v1 records.
    This class deliberately uses only the C++ standard library so its JSONL
    behaviour can be tested independently of a running OpenFOAM solver.

\*---------------------------------------------------------------------------*/

#ifndef Foam_PstreamMetricsWriter_H
#define Foam_PstreamMetricsWriter_H

#include <fstream>
#include <optional>
#include <string>

namespace Foam
{

class PstreamMetricsWriter
{
public:

    struct StartupState
    {
        int nodeCommsControl;
        int nodeCommsMin;
        bool usingNodeComms;
        int numNodes;
        int topologyControl;
        int configuredMpiBufferValue;
        std::optional<int> environmentMpiBufferBytesApplied;
        int attachedBufferBytes;
    };

    struct RuntimeState
    {
        bool floatTransfer;
        int nProcsSimpleSum;
        int nProcsNonblockingExchange;
        int tuningNbx;
        int pstreamBuffersAlgorithm;
        std::string defaultCommsType;
        int nPollProcInterfaces;
        int maxCommsSize;
        int nodeCommsControl;
        int nodeCommsMin;
        int topologyControl;
        bool usingNodeComms;
    };

private:

    std::ofstream stream_;
    std::string runId_;
    unsigned rank_;
    unsigned sequence_;
    bool active_;
    bool startupWritten_;
    bool runtimeWritten_;
    RuntimeState current_;

    static bool validRunId(const std::string& value) noexcept;
    bool writeRecord(const char* eventType, const std::string& payload) noexcept;

public:

    PstreamMetricsWriter
    (
        const std::string& directory,
        const std::string& runId,
        unsigned rank
    ) noexcept;

    ~PstreamMetricsWriter() noexcept;

    PstreamMetricsWriter(const PstreamMetricsWriter&) = delete;
    void operator=(const PstreamMetricsWriter&) = delete;

    bool active() const noexcept { return active_; }

    void writeStartup(const StartupState& state) noexcept;
    void writeRuntime(const RuntimeState& state) noexcept;
    void writeChanges(const RuntimeState& state) noexcept;
};

} // End namespace Foam

#endif

// ************************************************************************* //
