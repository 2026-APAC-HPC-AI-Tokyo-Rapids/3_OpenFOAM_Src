/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | www.openfoam.com
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2011-2014 OpenFOAM Foundation
    Copyright (C) 2016-2021 OpenCFD Ltd.
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

#include "smoothSolver.H"
#include "profiling.H"
#include "PrecisionAdaptor.H"
#include "../../lduMatrix/issue33Trace.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(smoothSolver, 0);

    lduMatrix::solver::addsymMatrixConstructorToTable<smoothSolver>
        addsmoothSolverSymMatrixConstructorToTable_;

    lduMatrix::solver::addasymMatrixConstructorToTable<smoothSolver>
        addsmoothSolverAsymMatrixConstructorToTable_;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::smoothSolver::smoothSolver
(
    const word& fieldName,
    const lduMatrix& matrix,
    const FieldField<Field, scalar>& interfaceBouCoeffs,
    const FieldField<Field, scalar>& interfaceIntCoeffs,
    const lduInterfaceFieldPtrsList& interfaces,
    const dictionary& solverControls
)
:
    lduMatrix::solver
    (
        fieldName,
        matrix,
        interfaceBouCoeffs,
        interfaceIntCoeffs,
        interfaces,
        solverControls
    )
{
    readControls();
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::smoothSolver::readControls()
{
    lduMatrix::solver::readControls();
    nSweeps_ = controlDict_.getOrDefault<label>("nSweeps", 1);
}


Foam::solverPerformance Foam::smoothSolver::solve
(
    scalarField& psi_s,
    const scalarField& source,
    const direction cmpt
) const
{
    PrecisionAdaptor<solveScalar, scalar> tpsi(psi_s);
    solveScalarField& psi = tpsi.ref();

    // Setup class containing solver performance data
    solverPerformance solverPerf(typeName, fieldName_);

    // If the nSweeps_ is negative do a fixed number of sweeps
    if (nSweeps_ < 0)
    {
        addProfiling(solve, "lduMatrix::smoother.", fieldName_);

        autoPtr<lduMatrix::smoother> smootherPtr = lduMatrix::smoother::New
        (
            fieldName_,
            matrix_,
            interfaceBouCoeffs_,
            interfaceIntCoeffs_,
            interfaces_,
            controlDict_
        );

        smootherPtr->smooth
        (
            psi,
            source,
            cmpt,
            -nSweeps_
        );

        solverPerf.nIterations() -= nSweeps_;
    }
    else
    {
        solveScalar normFactor = 0;
        solveScalarField residual;

        ConstPrecisionAdaptor<solveScalar, scalar> tsource(source);

        {
            solveScalarField Apsi(psi.size());
            solveScalarField temp(psi.size());

            // Calculate A.psi
            issue33TraceMark("smoothSolver.initialAmul.begin", fieldName_.c_str());
            matrix_.Amul(Apsi, psi, interfaceBouCoeffs_, interfaces_, cmpt);
            issue33TraceMark("smoothSolver.initialAmul.end", fieldName_.c_str());

            // Calculate normalisation factor
            issue33TraceMark("smoothSolver.normFactor.begin", fieldName_.c_str());
            normFactor = this->normFactor(psi, tsource(), Apsi, temp);
            issue33TraceMark("smoothSolver.normFactor.end", fieldName_.c_str());

            residual = tsource() - Apsi;

            matrix().setResidualField
            (
                ConstPrecisionAdaptor<scalar, solveScalar>(residual)(),
                fieldName_,
                true
            );

            // Calculate residual magnitude
            issue33TraceMark("smoothSolver.initialResidualMagnitude.begin", fieldName_.c_str());
            solverPerf.initialResidual() =
                gSumMag(residual, matrix().mesh().comm())/normFactor;
            issue33TraceMark("smoothSolver.initialResidualMagnitude.end", fieldName_.c_str());
            solverPerf.finalResidual() = solverPerf.initialResidual();
        }

        if ((log_ >= 2) || (lduMatrix::debug >= 2))
        {
            Info.masterStream(matrix().mesh().comm())
                << "   Normalisation factor = " << normFactor << endl;
        }


        // Check convergence, solve if not converged
        if
        (
            minIter_ > 0
         || !solverPerf.checkConvergence(tolerance_, relTol_, log_)
        )
        {
            addProfiling(solve, "lduMatrix::smoother.", fieldName_);

            autoPtr<lduMatrix::smoother> smootherPtr = lduMatrix::smoother::New
            (
                fieldName_,
                matrix_,
                interfaceBouCoeffs_,
                interfaceIntCoeffs_,
                interfaces_,
                controlDict_
            );

            // Smoothing loop
            do
            {
                issue33TraceMark("smoothSolver.smooth.begin", fieldName_.c_str());
                smootherPtr->smooth
                (
                    psi,
                    source,
                    cmpt,
                    nSweeps_
                );
                issue33TraceMark("smoothSolver.smooth.end", fieldName_.c_str());

                issue33TraceMark("smoothSolver.residual.begin", fieldName_.c_str());
                residual =
                    matrix_.residual
                    (
                        psi,
                        source,
                        interfaceBouCoeffs_,
                        interfaces_,
                        cmpt
                    );
                issue33TraceMark("smoothSolver.residual.end", fieldName_.c_str());

                // Calculate the residual to check convergence
                issue33TraceMark("smoothSolver.finalResidualMagnitude.begin", fieldName_.c_str());
                solverPerf.finalResidual() =
                    gSumMag(residual, matrix().mesh().comm())/normFactor;
                issue33TraceMark("smoothSolver.finalResidualMagnitude.end", fieldName_.c_str());
            } while
            (
                (
                    (solverPerf.nIterations() += nSweeps_) < maxIter_
                && !solverPerf.checkConvergence(tolerance_, relTol_, log_)
                )
             || solverPerf.nIterations() < minIter_
            );
        }

        matrix().setResidualField
        (
            ConstPrecisionAdaptor<scalar, solveScalar>(residual)(),
            fieldName_,
            false
        );
    }

    return solverPerf;
}


// ************************************************************************* //
