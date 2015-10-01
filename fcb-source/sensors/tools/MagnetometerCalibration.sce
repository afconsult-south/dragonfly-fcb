exec('GaussNewton.sce'); // working directory is assumed to be dir of this file.

// ============ entry point of script ==========================
// entry point means things run sequientially from here

// clc cmd deliberately commented out

// 6 samples with start-calibration-mag CLI function on FCB
// don't hold STM32F3 board near laptops or PCs - this will distort values.
MagSamples = [
// pos X
0.539090931, 0.0127272727, 0.00714285718
// neg X
-0.550909102, 0.0218181815, -0.0908163264
// neg Y
-0.0609090924, -0.463636369, 0.00714285718
// pos Y
0.0563636348, 0.612727284, -0.0693877563
// pos Z
-0.0254545454, 0.0118181817, 0.563265324
// neg Z
-0.0599999987, -0.0536363646, -0.48163265
];

// Initial guess of calibration vector
// beta[1-3] are offsets
// beta[4-6] are scaling
//
// x_j : calibrated value j = 1 .. 3 ( x y z axis)
// value : sensor sample
// beta_j, beta_j+3: offset, scaling3
//
// It is useful to choose the initial value with some care, since
// we are linearising a nonlinear function O(n^2), the GN method
// may not converge if the inital guess is too far from the solution.
//
// NOTE:
// offsets may not be zero - that will mean divide by zero in the betaChange
// calculation in function gaussNewtonLeastSquares!
calBeta = [-0.01; 0.01; 0.1; 1; 1; 1]; // N x 1 matrix, initial guess of beta

[sampleRows, sampleColumns] = size(MagSamples);
disp(MagSamples);
printf("sampleRows:%i sampleColumns:%i\n", sampleRows, sampleColumns);

if sampleColumns <> 3 then
    printf("ERROR: samples must be on Nx3 form\n");
    return;
end

// Earth's total magnetic field amplitude in Malmo see ref[2]
// then conversion of nano Tesla from above to Gauss
localMagField = 0.502559
nSamp = sampleRows;
normSamp = zeros(nSamp,3);
for i = 1:nSamp
   normSamp(i, 1:3) =  1 / localMagField * MagSamples(i, 1:3);
end

debugShowVar("normSamp", normSamp);

maxIterations = 10
calBeta = gaussNewtonLeastSquares(normSamp, calBeta, maxIterations);

// then convert beta offsets (calBeta[1..3])back to local magnetic vector ...
// the beta scaling (calBeta[4..6]) is already correct.
calBeta(1:3,1) = calBeta(1:3,1) * localMagField;
showVar("calBeta - denormalised", calBeta');

// and ... double check:
printf("Double Check:\n");
for i=1:nSamp
    calibrated(1:3) = [ (MagSamples(i, 1) - calBeta(1, 1)) * calBeta(4,1),
        (MagSamples(i, 2) - calBeta(2, 1)) * calBeta(5,1),
        (MagSamples(i, 3) - calBeta(3, 1)) * calBeta(6,1) ];
                            
    printf("norm(MagSamples[%i]): %f norm(calibrated[%i]):%f\n", i, norm(MagSamples(i, 1:3)),i, norm(calibrated));
end

// calBeta result: - 0.0127656    0.0804974    0.0338544    0.9025001    0.9189748    0.9415154 
