# ------------------  INPUTS TO MAIN PROGRAM  -------------------
max_step = 10

amrex.fpe_trap_invalid = 1

fabarray.mfiter_tile_size = 1024 1024 1024

erf.test_mapfactor = 1
erf.use_terrain = 0

# PROBLEM SIZE & GEOMETRY
geometry.prob_lo     = -12  -12  -1
geometry.prob_hi     =  12   12   1
amr.n_cell           =  48   48   4

geometry.is_periodic = 1 1 0

zlo.type = "SlipWall"
zhi.type = "SlipWall"

# TIME STEP CONTROL
erf.no_substepping  = 0
erf.fixed_dt        = 0.0003

# DIAGNOSTICS & VERBOSITY
erf.sum_interval    = 1       # timesteps between computing mass
erf.v               = 1       # verbosity in ERF.cpp
amr.v               = 1       # verbosity in Amr.cpp

# REFINEMENT / REGRIDDING
amr.max_level       = 0       # maximum level number allowed

# CHECKPOINT FILES
erf.check_file      = chk        # root name of checkpoint file
erf.check_int       = -100        # number of timesteps between checkpoints

# PLOTFILES
erf.plot_file_1     = plt        # prefix of plotfile name
erf.plot_int_1      = 100        # number of timesteps between plotfiles
erf.plot_vars_1     = density x_velocity y_velocity z_velocity pressure theta temp scalar

# SOLVER CHOICE
erf.alpha_T = 0.1
erf.alpha_C = 0.1
erf.use_gravity = false

erf.les_type         = "None"
erf.molec_diff_type  = "Constant"
erf.dynamicViscosity = 1.0

# PROBLEM PARAMETERS
prob.p_inf = 1e5  # reference pressure [Pa]
prob.T_inf = 300. # reference temperature [K]
prob.M_inf = 2.3904572186687872  # freestream Mach number [-]
prob.alpha = 0.7853981633974483  # inflow angle, 0 --> x-aligned [rad]
prob.beta  = 1.1088514254079065 # non-dimensional max perturbation strength [-]
prob.R     = 1.0  # characteristic length scale for grid [m]
prob.sigma = 1.0  # Gaussian standard deviation [-]
