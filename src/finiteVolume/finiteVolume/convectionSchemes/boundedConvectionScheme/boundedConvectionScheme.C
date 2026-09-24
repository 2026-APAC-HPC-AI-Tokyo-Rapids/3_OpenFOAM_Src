/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | www.openfoam.com
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2012-2016 OpenFOAM Foundation
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "boundedConvectionScheme.H"
#include "fvcSurfaceIntegrate.H"
#include "fvMatrices.H"
#include "fvmSup.H"
#include "issue33Trace.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

namespace
{

template<class Function>
auto issue33TraceDivPart
(
    bool enabled,
    const char* begin,
    const char* end,
    Function&& function
) -> decltype(function())
{
    if (enabled) issue33TraceMark(begin);
    auto result = function();
    if (enabled) issue33TraceMark(end);
    return result;
}

}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace fv
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class Type>
const convectionScheme<Type>& boundedConvectionScheme<Type>::scheme() const
{
    return scheme_();
}


template<class Type>
tmp<GeometricField<Type, fvsPatchField, surfaceMesh>>
boundedConvectionScheme<Type>::interpolate
(
    const surfaceScalarField& phi,
    const GeometricField<Type, fvPatchField, volMesh>& vf
) const
{
    return scheme_().interpolate(phi, vf);
}


template<class Type>
tmp<GeometricField<Type, fvsPatchField, surfaceMesh>>
boundedConvectionScheme<Type>::flux
(
    const surfaceScalarField& faceFlux,
    const GeometricField<Type, fvPatchField, volMesh>& vf
) const
{
    return scheme_().flux(faceFlux, vf);
}


template<class Type>
tmp<fvMatrix<Type>>
boundedConvectionScheme<Type>::fvmDiv
(
    const surfaceScalarField& faceFlux,
    const GeometricField<Type, fvPatchField, volMesh>& vf
) const
{
    const bool traceOmega = vf.name() == "omega";

    return
        issue33TraceDivPart
        (
            traceOmega,
            "omegaDiv.gauss.begin",
            "omegaDiv.gauss.end",
            [&]() { return scheme_().fvmDiv(faceFlux, vf); }
        )
      - issue33TraceDivPart
        (
            traceOmega,
            "omegaDiv.Sp.begin",
            "omegaDiv.Sp.end",
            [&]()
            {
                return fvm::Sp
                (
                    issue33TraceDivPart
                    (
                        traceOmega,
                        "omegaDiv.surfaceIntegrate.begin",
                        "omegaDiv.surfaceIntegrate.end",
                        [&]() { return fvc::surfaceIntegrate(faceFlux); }
                    ),
                    vf
                );
            }
        );
}


template<class Type>
tmp<GeometricField<Type, fvPatchField, volMesh>>
boundedConvectionScheme<Type>::fvcDiv
(
    const surfaceScalarField& faceFlux,
    const GeometricField<Type, fvPatchField, volMesh>& vf
) const
{
    return
        scheme_().fvcDiv(faceFlux, vf)
      - fvc::surfaceIntegrate(faceFlux)*vf;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fv

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
