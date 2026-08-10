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

#include "PstreamMetricsWriter.H"

#include <sstream>

namespace
{

const char* jsonBool(const bool value) noexcept
{
    return value ? "true" : "false";
}


std::string jsonString(const std::string& value)
{
    std::ostringstream os;
    os << '"';

    static const char hex[] = "0123456789abcdef";
    for (const unsigned char c : value)
    {
        switch (c)
        {
            case '"': os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b"; break;
            case '\f': os << "\\f"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default:
            {
                if (c < 0x20)
                {
                    os << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
                }
                else
                {
                    os << static_cast<char>(c);
                }
                break;
            }
        }
    }

    os << '"';
    return os.str();
}


template<class Type>
std::string changePayload
(
    const char* field,
    const Type& oldValue,
    const Type& newValue
)
{
    std::ostringstream os;
    os << "{\"field\":\"" << field << "\",\"old\":"
       << oldValue << ",\"new\":" << newValue << '}';
    return os.str();
}


template<>
std::string changePayload<bool>
(
    const char* field,
    const bool& oldValue,
    const bool& newValue
)
{
    std::ostringstream os;
    os << "{\"field\":\"" << field << "\",\"old\":"
       << jsonBool(oldValue) << ",\"new\":" << jsonBool(newValue) << '}';
    return os.str();
}


template<>
std::string changePayload<std::string>
(
    const char* field,
    const std::string& oldValue,
    const std::string& newValue
)
{
    std::ostringstream os;
    os << "{\"field\":\"" << field << "\",\"old\":"
       << jsonString(oldValue) << ",\"new\":" << jsonString(newValue) << '}';
    return os.str();
}

} // End anonymous namespace


bool Foam::PstreamMetricsWriter::validRunId
(
    const std::string& value
) noexcept
{
    if (value.size() != 34 || value[0] != 'r' || value[1] != '-')
    {
        return false;
    }

    for (std::size_t i = 2; i < value.size(); ++i)
    {
        const char c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
        {
            return false;
        }
    }

    return true;
}


Foam::PstreamMetricsWriter::PstreamMetricsWriter
(
    const std::string& directory,
    const std::string& runId,
    const unsigned rank
) noexcept
:
    stream_(),
    runId_(),
    rank_(rank),
    sequence_(0),
    active_(false),
    startupWritten_(false),
    runtimeWritten_(false),
    current_()
{
    try
    {
        if
        (
            directory.empty()
         || directory.front() != '/'
         || !validRunId(runId)
        )
        {
            return;
        }

        runId_ = runId;
        const std::string path
        (
            directory + "/rank-" + std::to_string(rank_) + ".jsonl"
        );
        stream_.open(path, std::ios::out | std::ios::app);
        active_ = stream_.good();
    }
    catch (...)
    {
        active_ = false;
    }
}


Foam::PstreamMetricsWriter::~PstreamMetricsWriter() noexcept
{
    try
    {
        if (stream_.is_open())
        {
            stream_.flush();
            stream_.close();
        }
    }
    catch (...)
    {}
}


bool Foam::PstreamMetricsWriter::writeRecord
(
    const char* eventType,
    const std::string& payload
) noexcept
{
    if (!active_)
    {
        return false;
    }

    try
    {
        stream_
            << "{\"schema_version\":\"openfoam-pstream-event/v1\""
            << ",\"run_id\":" << jsonString(runId_)
            << ",\"producer_id\":\"openfoam.pstream-hook/v1\""
            << ",\"rank\":" << rank_
            << ",\"sequence\":" << sequence_
            << ",\"event_type\":\"" << eventType << '"'
            << ",\"payload\":" << payload << "}\n";
        stream_.flush();

        if (!stream_.good())
        {
            active_ = false;
            return false;
        }

        ++sequence_;
        return true;
    }
    catch (...)
    {
        active_ = false;
        return false;
    }
}


void Foam::PstreamMetricsWriter::writeStartup
(
    const StartupState& state
) noexcept
{
    if (!active_ || startupWritten_ || runtimeWritten_)
    {
        return;
    }

    try
    {
        std::ostringstream os;
        os
            << "{\"node_comms_control\":" << state.nodeCommsControl
            << ",\"node_comms_min\":" << state.nodeCommsMin
            << ",\"using_node_comms\":" << jsonBool(state.usingNodeComms)
            << ",\"num_nodes\":" << state.numNodes
            << ",\"topology_control\":" << state.topologyControl
            << ",\"configured_mpi_buffer_value\":"
            << state.configuredMpiBufferValue
            << ",\"environment_mpi_buffer_bytes_applied\":";

        if (state.environmentMpiBufferBytesApplied)
        {
            os << *state.environmentMpiBufferBytesApplied;
        }
        else
        {
            os << "null";
        }

        os << ",\"attached_buffer_bytes\":" << state.attachedBufferBytes << '}';
        startupWritten_ = writeRecord("startup_applied", os.str());
    }
    catch (...)
    {
        active_ = false;
    }
}


void Foam::PstreamMetricsWriter::writeRuntime
(
    const RuntimeState& state
) noexcept
{
    if (!active_ || !startupWritten_ || runtimeWritten_)
    {
        return;
    }

    try
    {
        std::ostringstream os;
        os
            << "{\"float_transfer\":" << jsonBool(state.floatTransfer)
            << ",\"n_procs_simple_sum\":" << state.nProcsSimpleSum
            << ",\"n_procs_nonblocking_exchange\":"
            << state.nProcsNonblockingExchange
            << ",\"tuning_nbx\":" << state.tuningNbx
            << ",\"pstream_buffers_algorithm\":"
            << state.pstreamBuffersAlgorithm
            << ",\"default_comms_type\":" << jsonString(state.defaultCommsType)
            << ",\"n_poll_proc_interfaces\":" << state.nPollProcInterfaces
            << ",\"max_comms_size\":" << state.maxCommsSize
            << ",\"node_comms_control\":" << state.nodeCommsControl
            << ",\"node_comms_min\":" << state.nodeCommsMin
            << ",\"topology_control\":" << state.topologyControl
            << ",\"using_node_comms\":" << jsonBool(state.usingNodeComms)
            << '}';

        if (writeRecord("runtime_current", os.str()))
        {
            current_ = state;
            runtimeWritten_ = true;
        }
    }
    catch (...)
    {
        active_ = false;
    }
}


void Foam::PstreamMetricsWriter::writeChanges
(
    const RuntimeState& state
) noexcept
{
    if (!active_ || !runtimeWritten_)
    {
        return;
    }

    try
    {
        const auto writeChange = [this]
        (
            const char* field,
            const auto& oldValue,
            const auto& newValue
        )
        {
            if (active_ && oldValue != newValue)
            {
                writeRecord
                (
                    "runtime_change",
                    changePayload(field, oldValue, newValue)
                );
            }
        };

        writeChange
        (
            "float_transfer",
            current_.floatTransfer,
            state.floatTransfer
        );
        writeChange
        (
            "n_procs_simple_sum",
            current_.nProcsSimpleSum,
            state.nProcsSimpleSum
        );
        writeChange
        (
            "n_procs_nonblocking_exchange",
            current_.nProcsNonblockingExchange,
            state.nProcsNonblockingExchange
        );
        writeChange("tuning_nbx", current_.tuningNbx, state.tuningNbx);
        writeChange
        (
            "pstream_buffers_algorithm",
            current_.pstreamBuffersAlgorithm,
            state.pstreamBuffersAlgorithm
        );
        writeChange
        (
            "default_comms_type",
            current_.defaultCommsType,
            state.defaultCommsType
        );
        writeChange
        (
            "n_poll_proc_interfaces",
            current_.nPollProcInterfaces,
            state.nPollProcInterfaces
        );
        writeChange
        (
            "max_comms_size",
            current_.maxCommsSize,
            state.maxCommsSize
        );
        writeChange
        (
            "node_comms_control",
            current_.nodeCommsControl,
            state.nodeCommsControl
        );
        writeChange
        (
            "node_comms_min",
            current_.nodeCommsMin,
            state.nodeCommsMin
        );
        writeChange
        (
            "topology_control",
            current_.topologyControl,
            state.topologyControl
        );
        writeChange
        (
            "using_node_comms",
            current_.usingNodeComms,
            state.usingNodeComms
        );

        if (active_)
        {
            current_ = state;
        }
    }
    catch (...)
    {
        active_ = false;
    }
}

// ************************************************************************* //
