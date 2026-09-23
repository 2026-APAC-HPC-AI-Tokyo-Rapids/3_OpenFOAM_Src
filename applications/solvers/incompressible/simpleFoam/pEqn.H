{
    volScalarField rAU(1.0/UEqn.A());
    volVectorField HbyA(constrainHbyA(rAU*UEqn.H(), U, p));
    surfaceScalarField phiHbyA("phiHbyA", fvc::flux(HbyA));
    MRF.makeRelative(phiHbyA);
    adjustPhi(phiHbyA, U, p);

    tmp<volScalarField> rAtU(rAU);

    if (simple.consistent())
    {
        rAtU = 1.0/(1.0/rAU - UEqn.H1());
        phiHbyA +=
            fvc::interpolate(rAtU() - rAU)*fvc::snGrad(p)*mesh.magSf();
        HbyA -= (rAU - rAtU())*fvc::grad(p);
    }

    tUEqn.clear();

    // Update the pressure BCs to ensure flux consistency
    constrainPressure(p, U, phiHbyA, rAtU(), MRF);

    // Non-orthogonal pressure corrector loop
    while (simple.correctNonOrthogonal())
    {
        fvScalarMatrix pEqn
        (
            fvm::laplacian(rAtU(), p) == fvc::div(phiHbyA)
        );

        pEqn.setReference(pRefCell, pRefValue);

        issue33TraceMark("pEqn.solve.begin");
        pEqn.solve();
        issue33TraceMark("pEqn.solve.end");

        if (simple.finalNonOrthogonalIter())
        {
            issue33TraceMark("pEqn.flux.begin");
            phi = phiHbyA - pEqn.flux();
            issue33TraceMark("pEqn.flux.end");
        }
    }

    issue33TraceMark("pEqn.continuity.begin");
    #include "continuityErrs.H"
    issue33TraceMark("pEqn.continuity.end");

    // Explicitly relax pressure for momentum corrector
    issue33TraceMark("pEqn.pressureRelax.begin");
    p.relax();
    issue33TraceMark("pEqn.pressureRelax.end");

    // Momentum corrector
    issue33TraceMark("pEqn.momentumCorrection.begin");
    U = HbyA - rAtU()*fvc::grad(p);
    issue33TraceMark("pEqn.momentumCorrection.end");

    issue33TraceMark("pEqn.boundaryCorrection.begin");
    U.correctBoundaryConditions();
    issue33TraceMark("pEqn.boundaryCorrection.end");

    issue33TraceMark("pEqn.fvOptions.begin");
    fvOptions.correct(U);
    issue33TraceMark("pEqn.fvOptions.end");
}
