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
    Test-lduMatrixCSR

Description
    Verify that Amul lazily materializes CSR coefficients for a symmetric
    matrix, reuses them, and matches the face-loop result exactly.

\*---------------------------------------------------------------------------*/

#include "argList.H"
#include "lduMatrix.H"
#include "lduPrimitiveMesh.H"

using namespace Foam;


int main(int argc, char *argv[])
{
    argList::noParallel();
    argList args(argc, argv);

    labelList lowerAddr({0, 0, 1, 1, 2});
    labelList upperAddr({1, 3, 2, 3, 3});

    lduPrimitiveMesh mesh
    (
        4,
        lowerAddr,
        upperAddr,
        UPstream::worldComm,
        false
    );

    const scalarField diagCoeffs({4.125, -2.5, 7.75, 0.125});
    const scalarField upperCoeffs({0.5, -1.25, 2.75, 4.5, -3.125});

    lduMatrix csrMatrix(mesh);
    csrMatrix.diag() = diagCoeffs;
    csrMatrix.upper() = upperCoeffs;

    lduMatrix faceMatrix(mesh);
    faceMatrix.diag() = diagCoeffs;
    faceMatrix.upper() = upperCoeffs;
    faceMatrix.lower() = upperCoeffs;

    if (!csrMatrix.symmetric() || csrMatrix.hasLowerCSR())
    {
        InfoErr<< "Symmetric matrix was not initially CSR-lazy" << nl;
        return 1;
    }

    const FieldField<Field, scalar> interfaceCoeffs;
    const lduInterfaceFieldPtrsList interfaces;
    const solveScalarField psi({1.5, -2.25, 0.75, 3.5});

    solveScalarField csrResult(psi.size());
    solveScalarField faceResult(psi.size());

    csrMatrix.Amul(csrResult, psi, interfaceCoeffs, interfaces, 0);
    faceMatrix.Amul(faceResult, psi, interfaceCoeffs, interfaces, 0);

    if
    (
        !csrMatrix.hasLowerCSR()
     || faceMatrix.hasLowerCSR()
     || csrResult != faceResult
    )
    {
        InfoErr<< "CSR path selection or result check failed:" << nl
            << "CSR  " << csrResult << nl
            << "face " << faceResult << nl;
        return 1;
    }

    const scalar* const cachedCoeffs = csrMatrix.lowerCSR().cdata();

    csrMatrix.Amul(csrResult, psi, interfaceCoeffs, interfaces, 0);

    if (csrMatrix.lowerCSR().cdata() != cachedCoeffs)
    {
        InfoErr<< "Amul rebuilt the cached CSR coefficients" << nl;
        return 1;
    }

    Info<< "Symmetric Amul CSR materialization and reuse: pass" << nl;
    return 0;
}


// ************************************************************************* //
