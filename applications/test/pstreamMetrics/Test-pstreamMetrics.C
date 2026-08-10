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

Application
    Test-pstreamMetrics

Description
    Unit test for the dependency-free Pstream JSONL writer.

\*---------------------------------------------------------------------------*/

#include "PstreamMetricsWriter.H"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{

bool expect(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

} // End anonymous namespace


int main()
{
    bool passed = true;
    char directoryTemplate[] = "/tmp/openfoam-pstream-metrics-XXXXXX";
    const char* directoryName = ::mkdtemp(directoryTemplate);
    if (!directoryName)
    {
        std::perror("mkdtemp");
        return 1;
    }

    const std::string directory(directoryName);
    const std::string runId("r-0123456789abcdef0123456789abcdef");
    const std::string output(directory + "/rank-7.jsonl");

    {
        Foam::PstreamMetricsWriter invalid(directory, "not-a-run-id", 6);
        passed &= expect(!invalid.active(), "invalid run ID must disable writer");

        Foam::PstreamMetricsWriter missing
        (
            directory + "/not-created",
            runId,
            6
        );
        passed &= expect(!missing.active(), "writer must not create sink directory");

        Foam::PstreamMetricsWriter writer(directory, runId, 7);
        passed &= expect(writer.active(), "valid writer must open rank file");

        writer.writeStartup
        ({1, 0, false, 2, -1, -5, std::nullopt, 20000000});

        Foam::PstreamMetricsWriter::RuntimeState runtime
        {
            false,
            8,
            16,
            -2,
            0,
            "nonBlocking",
            4,
            -10,
            1,
            0,
            -1,
            false
        };
        writer.writeRuntime(runtime);

        // No-op observation must not produce a runtime_change event.
        writer.writeChanges(runtime);
        runtime.floatTransfer = true;
        runtime.defaultCommsType = "scheduled";
        writer.writeChanges(runtime);
    }

    std::ifstream input(output);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);)
    {
        lines.push_back(std::move(line));
    }

    passed &= expect(lines.size() == 4, "expected two sentinels and two changes");
    if (lines.size() == 4)
    {
        const std::string prefix
        (
            "{\"schema_version\":\"openfoam-pstream-event/v1\","
            "\"run_id\":\"" + runId + "\","
            "\"producer_id\":\"openfoam.pstream-hook/v1\",\"rank\":7,"
        );

        passed &= expect
        (
            lines[0] == prefix
              + "\"sequence\":0,\"event_type\":\"startup_applied\","
                "\"payload\":{\"node_comms_control\":1,\"node_comms_min\":0,"
                "\"using_node_comms\":false,\"num_nodes\":2,"
                "\"topology_control\":-1,\"configured_mpi_buffer_value\":-5,"
                "\"environment_mpi_buffer_bytes_applied\":null,"
                "\"attached_buffer_bytes\":20000000}}",
            "startup_applied closed payload"
        );
        passed &= expect
        (
            lines[1] == prefix
              + "\"sequence\":1,\"event_type\":\"runtime_current\","
                "\"payload\":{\"float_transfer\":false,"
                "\"n_procs_simple_sum\":8,"
                "\"n_procs_nonblocking_exchange\":16,\"tuning_nbx\":-2,"
                "\"pstream_buffers_algorithm\":0,"
                "\"default_comms_type\":\"nonBlocking\","
                "\"n_poll_proc_interfaces\":4,\"max_comms_size\":-10,"
                "\"node_comms_control\":1,\"node_comms_min\":0,"
                "\"topology_control\":-1,\"using_node_comms\":false}}",
            "runtime_current closed payload"
        );
        passed &= expect
        (
            lines[2] == prefix
              + "\"sequence\":2,\"event_type\":\"runtime_change\","
                "\"payload\":{\"field\":\"float_transfer\","
                "\"old\":false,\"new\":true}}",
            "boolean runtime_change payload"
        );
        passed &= expect
        (
            lines[3] == prefix
              + "\"sequence\":3,\"event_type\":\"runtime_change\","
                "\"payload\":{\"field\":\"default_comms_type\","
                "\"old\":\"nonBlocking\",\"new\":\"scheduled\"}}",
            "string runtime_change payload"
        );
    }

    std::remove(output.c_str());
    ::rmdir(directory.c_str());

    if (passed)
    {
        std::cout << "Pstream metrics writer tests passed\n";
        return 0;
    }
    return 1;
}

// ************************************************************************* //
