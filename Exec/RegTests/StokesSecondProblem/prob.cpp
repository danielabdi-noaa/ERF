#include "prob.H"
#include "TerrainMetrics.H"

using namespace amrex;

std::unique_ptr<ProblemBase>
amrex_probinit(const amrex_real* problo, const amrex_real* probhi)
{
    return std::make_unique<Problem>(problo, probhi);
}

Problem::Problem()
{
  // Parse params
  ParmParse pp("prob");
  pp.query("rho_0", parms.rho_0);

  init_base_parms(parms.rho_0, parms.T_0);
}

void
Problem::init_custom_pert(
    const Box& bx,
    const Box& xbx,
    const Box& ybx,
    const Box& zbx,
    Array4<Real      > const& state,
    Array4<Real      > const& x_vel,
    Array4<Real      > const& y_vel,
    Array4<Real      > const& z_vel,
    Array4<Real      > const&,
    Array4<Real      > const& p_hse,
    Array4<Real const> const&,
    Array4<Real const> const&,
    GeometryData const& geomdata,
    Array4<Real const> const& /*mf_m*/,
    Array4<Real const> const& /*mf_u*/,
    Array4<Real const> const& /*mf_v*/,
    const SolverChoice& sc)
{
    const bool use_moisture = (sc.moisture_type != MoistureType::None);

    ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) noexcept
    {
        // This version perturbs rho but not p -- TODO: CHECK THIS
        state(i, j, k, RhoTheta_comp) = std::pow(1.0,1.0/Gamma) * 101325.0 / 287.0 - p_hse(i,j,k);

        // Set scalar = 0 everywhere
        state(i, j, k, RhoScalar_comp) = 0.0;

        if (use_moisture) {
            state(i, j, k, RhoQ1_comp) = 0.0;
            state(i, j, k, RhoQ2_comp) = 0.0;
        }
    });

    // Set the x-velocity
    ParallelFor(xbx, [=] AMREX_GPU_DEVICE(int i, int j, int k) noexcept
    {
        x_vel(i, j, k) = 0.0;
    });

    // Set the y-velocity
    ParallelFor(ybx, [=] AMREX_GPU_DEVICE(int i, int j, int k) noexcept
    {
        y_vel(i, j, k) = 0.0;
    });

    // Set the z-velocity from impenetrable condition
    ParallelFor(zbx, [=] AMREX_GPU_DEVICE(int i, int j, int k) noexcept
    {
        z_vel(i, j, k) = 0.0;
    });

    amrex::Gpu::streamSynchronize();

}

Real
Problem::compute_terrain_velocity(const Real time)
{
    Real U = 10.0;
    Real omega = 2.0*M_PI*1000.0;
    return U*cos(omega*time);
}
