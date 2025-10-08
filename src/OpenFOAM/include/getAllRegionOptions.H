/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | www.openfoam.com
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2021-2025 OpenCFD Ltd.
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM, distributed under GPL-3.0-or-later.

Description
    Gets single or multiple region names based on command-line options:
    (-all-regions | -regions | -regions)

Priority
    1. -all-regions (-allRegions)
    2. -regions = specify multiple regions to select, or a single region
    3. -region = specify a single region

Note
    There is no semantical difference between "-regions name"
    and "-region name".
    If regionProperties is missing (or empty) and the "-regions"
    option is supplied with literal names, these will be used directly
    instead of being used to filter the regionProperties.

Required Classes
    - Foam::polyMesh
    - Foam::regionProperties
    - Foam::IOobjectOption

Required Variables
    - args [argList]
    - runTime [Time]

Provides Variables
    - regionNames [wordList]

See Also
    addAllRegionOptions.H

\*---------------------------------------------------------------------------*/

wordList regionNames;
{
    // Local alias
    auto& names = regionNames;
    // Exit or fallback if nothing matches
    constexpr bool exitOnNoMatches = true;

    if (args.found("all-regions"))
    {
        names =
        (
            Foam::regionProperties
            (
                runTime,
                IOobjectOption::READ_IF_PRESENT
            ).names()
        );

        if (names.empty())
        {
            InfoErr
                << "Warning: No regionProperties, "
                   "assume default region" << nl << endl;
        }
        else
        {
            Info<< "Using all regions: "
                << flatOutput(names) << nl;
        }
    }
    else if
    (
        wordRes select;
        args.readListIfPresent<wordRe>("regions", select)
    )
    {
        select.uniq();  // Normally a no-op

        if (select.size() == 1 && select.front().isLiteral())
        {
            // Identical to -region NAME
            names.resize(1);
            names.front() = select.front();
        }
        else if (select.size())
        {
            names =
            (
                Foam::regionProperties
                (
                    runTime,
                    IOobjectOption::READ_IF_PRESENT
                ).names()
            );

            if (!names.empty())
            {
                // Filter the region names
                const auto indices = select.matching(names);

                if (indices.empty())
                {
                    InfoErr
                        << "No match in regions: "
                        << flatOutput(names) << nl;

                    if constexpr (exitOnNoMatches)
                    {
                        InfoErr<< "... stopping" << nl << endl;
                        return 1;
                    }
                    else
                    {
                        InfoErr<< "... ignoring and using default region"
                            << nl << endl;

                        // Arbitrary fallback, but can handle this further on
                        names.resize(1);
                        names.front() = Foam::polyMesh::defaultRegion;
                    }
                }
                else
                {
                    names = wordList(names, indices);
                    Info<< "Using regions: "
                        << flatOutput(names) << nl;
                }
            }
            else
            {
                // No information from regionProperties:
                // - use literal names from select

                names.resize(select.size());

                label nGood(0);
                for (const auto& val : select)
                {
                    if (val.isLiteral())
                    {
                        names[nGood++] = val;
                    }
                }
                if (nGood)
                {
                    names.resize(nGood);

                    InfoErr
                        << "Warning: No regionProperties, "
                           "using specified regions" << nl;

                    Info<< "Using regions: "
                        << flatOutput(names) << nl;
                }
                else
                {
                    names.resize(1);
                    names.front() = Foam::polyMesh::defaultRegion;

                    InfoErr
                        << "Warning: No regionProperties, "
                           "assume default region" << nl << endl;
                }
            }
        }
    }
    else if (args.found("region"))
    {
        names.resize(1);
        names.front() = args.get<word>("region");
    }

    // Fallback to defaultRegion
    if (names.empty())
    {
        names.resize(1);
        names.front() = Foam::polyMesh::defaultRegion;
    }
}


// ************************************************************************* //
