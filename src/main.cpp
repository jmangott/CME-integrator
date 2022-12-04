#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <generic/matrix.hpp>
#include <generic/storage.hpp>
#include <lr/coefficients.hpp>
#include <lr/lr.hpp>

#include "grid_class.hpp"
#include "index_functions.hpp"
#include "io_functions.hpp"
#include "kl_step_functions.hpp"
#include "parameters.hpp"
#include "reactions.hpp"
#include "s_step_functions.hpp"

using std::cout;
using std::endl;
using std::string;
using std::vector;


int main()
{
    /////////////////////////////////////////////
    /////////////////// SETUP ///////////////////
    /////////////////////////////////////////////

    grid_info grid(kM1, kM2, kR, kN, kK);

    // Declare LR-specific objects
    // Temporary objects for multiplication
    multi_array<double, 2> tmp_x1({grid.dx1, grid.r});
    multi_array<double, 2> tmp_x2({grid.dx2, grid.r});

    // Objects for setting up X1 and X2 for t = 0
    multi_array<double, 2> xx1({grid.dx1, grid.r});
    multi_array<double, 2> xx2({grid.dx2, grid.r});
    vector<const double *> x1, x2;

    // Low rank structure (for storing X1, X2 and S)
    lr2<double> lr_sol(grid.r, {grid.dx1, grid.dx2});

    // Inner products
    std::function<double(double *, double *)> ip_xx1;
    std::function<double(double *, double *)> ip_xx2;
    blas_ops blas;
    gram_schmidt gs(&blas);

    vector<Index> sigma1, sigma2;

    ReadInMultiArray(xx1, "../input/x1_input.csv");
    ReadInMultiArray(xx2, "../input/x2_input.csv");

    // Point to the beginning of every column of X1 and X2
    double *it1 = xx1.begin();
    double *it2 = xx2.begin();
    for (int i = 0; i < kR; i++)
    {
        x1.push_back(it1);
        x2.push_back(it2);
        it1 += kM1 * grid.n1(0);
        it2 += kM2 * grid.n2(0);
    }

    // Set up the low-rank structure and the inner products
    double h_xx_mult1 = 1, h_xx_mult2 = 1;
    for (Index i = 0; i < grid.m1; i++)
        h_xx_mult1 *= grid.h1(i);
    for (Index i = 0; i < grid.m2; i++)
        h_xx_mult2 *= grid.h2(i);

    ip_xx1 = inner_product_from_const_weight(h_xx_mult1, grid.dx1);
    ip_xx2 = inner_product_from_const_weight(h_xx_mult2, grid.dx2);
    initialize(lr_sol, x1, x2, ip_xx1, ip_xx2, blas);

    // Calculate the shift amount for all reactions (this has to be done only once)
    CalculateShiftAmount(sigma1, sigma2, mysystem, grid);

    double t = 0.0;
    for(Index ts = 0; ts < kNsteps; ts++) 
    {
        if(kTstar - t < kTau)
            kTau = kTstar - t;

        /////////////////////////////////////////////
        ////////////////// K-STEP ///////////////////
        /////////////////////////////////////////////

        tmp_x1 = lr_sol.X;
        blas.matmul(tmp_x1, lr_sol.S, lr_sol.X); // lr_sol.X contains now K
        PerformKLStep(2, sigma1, sigma2, lr_sol, blas, mysystem, grid, kTau);
        // Perform the QR decomposition K = X * S
        gs(lr_sol.X, lr_sol.S, ip_xx1);

        /////////////////////////////////////////////
        ////////////////// S-STEP ///////////////////
        /////////////////////////////////////////////

        PerformSStep(sigma1, sigma2, lr_sol, blas, mysystem, grid, kTau);

        /////////////////////////////////////////////
        ////////////////// L-STEP ///////////////////
        /////////////////////////////////////////////

        tmp_x2 = lr_sol.V;
        blas.matmul_transb(tmp_x2, lr_sol.S, lr_sol.V); // lr_sol.V contains now L
        PerformKLStep(1, sigma1, sigma2, lr_sol, blas, mysystem, grid, kTau);
        // Perform the QR decomposition L = V * S^T
        gs(lr_sol.V, lr_sol.S, ip_xx2);
        transpose_inplace(lr_sol.S);

        t += kTau;
    }

    // Write output files
    WriteOutMultiArray(lr_sol.X, "../output/x1_output_0");
    WriteOutMultiArray(lr_sol.S, "../output/s_output_0");
    WriteOutMultiArray(lr_sol.V, "../output/x2_output_0");

    return 0;
}