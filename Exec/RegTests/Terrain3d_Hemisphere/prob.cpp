#include "prob.H"
#include "TerrainMetrics.H"

using namespace amrex;

std::unique_ptr<ProblemBase>
amrex_probinit (const amrex_real* problo,
                const amrex_real* probhi)
{
    return std::make_unique<Problem>(problo, probhi);
}

Problem::Problem (const amrex::Real* problo,
                  const amrex::Real* probhi)
{
    // Parse params
    ParmParse pp("prob");
    pp.query("rho_0", parms.rho_0);
    pp.query("U_0", parms.U_0);
    pp.query("U_0_Pert_Mag", parms.U_0_Pert_Mag);
    pp.query("V_0_Pert_Mag", parms.V_0_Pert_Mag);
    pp.query("W_0_Pert_Mag", parms.W_0_Pert_Mag);
    pp.query("pert_ref_height", parms.pert_ref_height);
    parms.aval = parms.pert_periods_U * 2.0 * PI / (probhi[1] - problo[1]);
    parms.bval = parms.pert_periods_V * 2.0 * PI / (probhi[0] - problo[0]);
    parms.ufac = parms.pert_deltaU * std::exp(0.5) / parms.pert_ref_height;
    parms.vfac = parms.pert_deltaV * std::exp(0.5) / parms.pert_ref_height;

    init_base_parms(parms.rho_0, parms.T_0);
}

void
Problem::init_custom_pert (
    const Box& bx,
    const Box& xbx,
    const Box& ybx,
    const Box& zbx,
    Array4<Real const> const& /*state*/,
    Array4<Real      > const& state_pert,
    Array4<Real      > const& x_vel_pert,
    Array4<Real      > const& y_vel_pert,
    Array4<Real      > const& z_vel_pert,
    Array4<Real      > const& /*r_hse*/,
    Array4<Real      > const& /*p_hse*/,
    Array4<Real const> const& z_nd,
    Array4<Real const> const& /*z_cc*/,
    GeometryData const& geomdata,
    Array4<Real const> const& /*mf_m*/,
    Array4<Real const> const& /*mf_u*/,
    Array4<Real const> const& /*mf_v*/,
    const SolverChoice& sc)
{
    const int khi = geomdata.Domain().bigEnd()[2];

    const bool use_moisture = (sc.moisture_type != MoistureType::None);
    const bool use_terrain  = sc.use_terrain;

    AMREX_ALWAYS_ASSERT(bx.length()[2] == khi+1);

    // Geometry (note we must include these here to get the data on device)
    ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) noexcept
    {
        // Set scalar = 0 everywhere
        state_pert(i, j, k, RhoScalar_comp) = 0.0;

        if (use_moisture) {
            state_pert(i, j, k, RhoQ1_comp) = 0.0;
            state_pert(i, j, k, RhoQ2_comp) = 0.0;
        }
    });

    // Set the x-velocity
    ParallelForRNG(xbx, [=, parms_d=parms] AMREX_GPU_DEVICE(int i, int j, int k, const amrex::RandomEngine& engine) noexcept
    {
        const Real* prob_lo = geomdata.ProbLo();
        const Real* dx = geomdata.CellSize();
        const Real z = use_terrain ? 0.25*( z_nd(i,j  ,k) + z_nd(i,j  ,k+1)
                                          + z_nd(i,j+1,k) + z_nd(i,j+1,k+1) )
            : prob_lo[2] + (k + 0.5) * dx[2];

        x_vel(i, j, k) = 10.0;
        if ((z <= parms_d.pert_ref_height) && (parms_d.U_0_Pert_Mag != 0.0))
        {
            Real rand_double = amrex::Random(engine); // Between 0.0 and 1.0
            Real x_vel_prime = (rand_double*2.0 - 1.0)*parms_d.U_0_Pert_Mag;
            x_vel_pert(i, j, k) += x_vel_prime;
        }
    });

    // Set the y-velocity
    ParallelForRNG(ybx, [=, parms_d=parms] AMREX_GPU_DEVICE(int i, int j, int k, const amrex::RandomEngine& engine) noexcept
    {
        const Real* prob_lo = geomdata.ProbLo();
        const Real* dx = geomdata.CellSize();
        const Real x = prob_lo[0] + (i + 0.5) * dx[0];
        const Real z = use_terrain ? 0.25*( z_nd(i  ,j,k) + z_nd(i  ,j,k+1)
                                          + z_nd(i+1,j,k) + z_nd(i+1,j,k+1) )
            : prob_lo[2] + (k + 0.5) * dx[2];

        // Set the y-velocity
        y_vel_pert(i, j, k) = 0.0;
        if ((z <= parms_d.pert_ref_height) && (parms_d.V_0_Pert_Mag != 0.0))
        {
            Real rand_double = amrex::Random(engine); // Between 0.0 and 1.0
            Real y_vel_prime = (rand_double*2.0 - 1.0)*parms_d.V_0_Pert_Mag;
            y_vel_pert(i, j, k) += y_vel_prime;
        }
        if (parms_d.pert_deltaV != 0.0)
        {
            const amrex::Real xl = x - prob_lo[0];
            const amrex::Real zl = z / parms_d.pert_ref_height;
            const amrex::Real damp = std::exp(-0.5 * zl * zl);
            y_vel_pert(i, j, k) += parms_d.vfac * damp * z * std::cos(parms_d.bval * xl);
        }
    });

    const auto dx = geomdata.CellSize();
    amrex::GpuArray<Real, AMREX_SPACEDIM> dxInv;
    dxInv[0] = 1. / dx[0];
    dxInv[1] = 1. / dx[1];
    dxInv[2] = 1. / dx[2];

    // Set the z-velocity from impenetrable condition
    ParallelForRNG(zbx, [=, parms_d=parms] AMREX_GPU_DEVICE(int i, int j, int k, const amrex::RandomEngine& engine) noexcept
    {
        const int dom_lo_z = geomdata.Domain().smallEnd()[2];
        const int dom_hi_z = geomdata.Domain().bigEnd()[2];

        // Set the z-velocity
        if (k == dom_lo_z || k == dom_hi_z+1)
        {
            z_vel_pert(i, j, k) = 0.0;
        }
        else if (parms_d.W_0_Pert_Mag != 0.0)
        {
            Real rand_double = amrex::Random(engine); // Between 0.0 and 1.0
            Real z_vel_prime = (rand_double*2.0 - 1.0)*parms_d.W_0_Pert_Mag;
            z_vel_pert(i, j, k) = z_vel_prime;
        }
    });

    amrex::Gpu::streamSynchronize();
}

void
Problem::init_custom_terrain (
    const Geometry& geom,
    MultiFab& z_phys_nd,
    const Real& time)
{
    // Check if a valid text file exists for the terrain
    std::string fname;
    ParmParse pp("erf");
    auto valid_fname = pp.query("terrain_file_name",fname);
    if (valid_fname) {
        this->read_custom_terrain(fname,geom,z_phys_nd,time);
    } else {

        // Domain cell size and real bounds
        auto dx = geom.CellSizeArray();
        auto ProbLoArr = geom.ProbLoArray();
        auto ProbHiArr = geom.ProbHiArray();

        // Domain valid box (z_nd is nodal)
        const amrex::Box& domain = geom.Domain();
        int domlo_x = domain.smallEnd(0); int domhi_x = domain.bigEnd(0) + 1;
        int domlo_y = domain.smallEnd(1); int domhi_y = domain.bigEnd(1) + 1;
        int domlo_z = domain.smallEnd(2);

        // User function parameters
        Real a    = 0.5;
        Real xcen = 0.5 * (ProbLoArr[0] + ProbHiArr[0]);
        Real ycen = 0.5 * (ProbLoArr[1] + ProbHiArr[1]);

        // Number of ghost cells
        int ngrow = z_phys_nd.nGrow();

        // Populate bottom plane
        int k0 = domlo_z;

        for ( amrex::MFIter mfi(z_phys_nd,amrex::TilingIfNotGPU()); mfi.isValid(); ++mfi )
        {
            // Grown box with no z range
            amrex::Box xybx = mfi.growntilebox(ngrow);
            xybx.setRange(2,0);

            amrex::Array4<Real> const& z_arr = z_phys_nd.array(mfi);

            ParallelFor(xybx, [=] AMREX_GPU_DEVICE (int i, int j, int)
            {

                // Clip indices for ghost-cells
                int ii = amrex::min(amrex::max(i,domlo_x),domhi_x);
                int jj = amrex::min(amrex::max(j,domlo_y),domhi_y);

                // Location of nodes
                Real x = (ii  * dx[0] - xcen);
                Real y = (jj  * dx[1] - ycen);

                if(std::pow(x*x + y*y, 0.5) < a){
                    z_arr(i,j,k0) = std::pow(a*a - x*x - y*y , 0.5);
                }
                else{
                    z_arr(i,j,k0) = 0.0;
                }

            });
        }
    }
}
