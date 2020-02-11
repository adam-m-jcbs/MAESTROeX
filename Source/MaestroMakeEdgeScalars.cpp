
#include <Maestro.H>
#include <Maestro_F.H>

using namespace amrex;

void
Maestro::MakeEdgeScal (Vector<MultiFab>& state,
                       Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sedge,
                       Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac,
                       Vector<MultiFab>& force,
                       int is_vel, const Vector<BCRec>& bcs, int nbccomp,
                       int start_scomp, int start_bccomp, int num_comp, int is_conservative)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeEdgeScal()", MakeEdgeScal);

    for (int lev=0; lev<=finest_level; ++lev) {

        // Get the index space and grid spacing of the domain
        const Box& domainBox = geom[lev].Domain();
        const Real* dx = geom[lev].CellSize();

        // get references to the MultiFabs at level lev
        const MultiFab& scal_mf = state[lev];

        MultiFab Ip, Im, Ipf, Imf;
        Ip.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Im.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Ipf.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Imf.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
	
        MultiFab slx, srx, simhx;
        slx.define(grids[lev],dmap[lev],1,1);
        srx.define(grids[lev],dmap[lev],1,1);
        simhx.define(grids[lev],dmap[lev],1,1);

        MultiFab sly, sry, simhy;
        sly.define(grids[lev],dmap[lev],1,1);
        sry.define(grids[lev],dmap[lev],1,1);
        simhy.define(grids[lev],dmap[lev],1,1);

        slx.setVal(0.);
        srx.setVal(0.);
        simhx.setVal(0.);
        sly.setVal(0.);
        sry.setVal(0.);
        simhy.setVal(0.);

#if (AMREX_SPACEDIM == 3)

        MultiFab slopez, divu;
        slopez.define(grids[lev],dmap[lev],1,1);
        divu.define(grids[lev],dmap[lev],1,1);

        MultiFab slz, srz, simhz;
        slz.define(grids[lev],dmap[lev],1,1);
        srz.define(grids[lev],dmap[lev],1,1);
        simhz.define(grids[lev],dmap[lev],1,1);

        MultiFab simhxy, simhxz, simhyx, simhyz, simhzx, simhzy;
        simhxy.define(grids[lev],dmap[lev],1,1);
        simhxz.define(grids[lev],dmap[lev],1,1);
        simhyx.define(grids[lev],dmap[lev],1,1);
        simhyz.define(grids[lev],dmap[lev],1,1);
        simhzx.define(grids[lev],dmap[lev],1,1);
        simhzy.define(grids[lev],dmap[lev],1,1);

        slx.setVal(0.);
        srx.setVal(0.);
        simhx.setVal(0.);
        sly.setVal(0.);
        sry.setVal(0.);
        simhy.setVal(0.);
        slz.setVal(0.);
        srz.setVal(0.);
        simhz.setVal(0.);

        simhxy.setVal(0.);
        simhxz.setVal(0.);
        simhyx.setVal(0.);
        simhyz.setVal(0.);
        simhzx.setVal(0.);
        simhzy.setVal(0.);
#endif

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
#if (AMREX_SPACEDIM == 2)

        Vector<MultiFab> vec_scal_mf(num_comp);
        for (int comp=0; comp < num_comp; ++comp) {
            vec_scal_mf[comp].define(grids[lev],dmap[lev],1,scal_mf.nGrow());

            MultiFab::Copy(vec_scal_mf[comp], scal_mf, start_scomp+comp, 0, 1, scal_mf.nGrow());
        }
#ifdef _OPENMP
#pragma omp parallel
#endif
        for ( MFIter mfi(scal_mf, TilingIfNotGPU()); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& obx = amrex::grow(tileBox, 1);

            // Be careful to pass in comp+1 for fortran indexing
            for (int scomp = start_scomp; scomp < start_scomp + num_comp; ++scomp) {

                int vcomp = scomp - start_scomp;
                int bccomp = start_bccomp + scomp - start_scomp;

                Array4<Real> const scal_arr = state[lev].array(mfi);

                Array4<Real> const umac_arr = umac[lev][0].array(mfi);
                Array4<Real> const vmac_arr = umac[lev][1].array(mfi);

                Array4<Real> const slx_arr = slx.array(mfi);
                Array4<Real> const srx_arr = srx.array(mfi);
                Array4<Real> const sly_arr = sly.array(mfi);
                Array4<Real> const sry_arr = sry.array(mfi);

                Array4<Real> const simhx_arr = simhx.array(mfi);
                Array4<Real> const simhy_arr = simhy.array(mfi);

                if (ppm_type == 0) {
                    // we're going to reuse Ip here as slopex and Im as slopey
                    // as they have the correct number of ghost zones

                    // x-direction
                    Slopex(obx, vec_scal_mf[vcomp].array(mfi), 
                           Ip.array(mfi), 
                           domainBox, bcs, 
                           1,bccomp);

                    // y-direction
                    Slopey(obx, vec_scal_mf[vcomp].array(mfi), 
                           Im.array(mfi), 
                           domainBox, bcs, 
                           1,bccomp);

                } else {

                    PPM(obx, scal_arr, 
                        umac_arr, vmac_arr, 
                        Ip.array(mfi), Im.array(mfi), 
                        domainBox, bcs, dx, 
                        true, scomp, bccomp);

                    if (ppm_trace_forces == 1) {

                        PPM(obx, force[lev].array(mfi), 
                            umac_arr, vmac_arr, 
                            Ipf.array(mfi), Imf.array(mfi), 
                            domainBox, bcs, dx, 
                            true, scomp, bccomp);
                    }
                }

                // Create s_{\i-\half\e_x}^x, etc.

                MakeEdgeScalPredictor(mfi, slx_arr, srx_arr,
                                      sly_arr, sry_arr,
                                      scal_arr, 
                                      Ip.array(mfi), Im.array(mfi), 
                                      umac_arr, vmac_arr, 
                                      simhx_arr, simhy_arr, 
                                      domainBox, bcs, dx,
                                      scomp, bccomp, is_vel);

                Array4<Real> const sedgex_arr = sedge[lev][0].array(mfi);
                Array4<Real> const sedgey_arr = sedge[lev][1].array(mfi);

                // Create sedgelx, etc.

                MakeEdgeScalEdges(mfi, slx_arr, srx_arr,
                                  sly_arr, sry_arr,
                                  scal_arr, 
                                  sedgex_arr, sedgey_arr, 
                                  force[lev].array(mfi),
                                  umac_arr, vmac_arr, 
                                  Ipf.array(mfi), Imf.array(mfi),
                                  simhx_arr, simhy_arr, 
                                  domainBox, bcs, dx,
                                  scomp, bccomp, 
                                  is_vel, is_conservative);
            } // end loop over components
        } // end MFIter loop

#elif (AMREX_SPACEDIM == 3)

        Vector<MultiFab> vec_scal_mf(num_comp);
        for (int comp=0; comp < num_comp; ++comp) {
            vec_scal_mf[comp].define(grids[lev],dmap[lev],1,scal_mf.nGrow());
            vec_scal_mf[comp].setVal(0.);

            MultiFab::Copy(vec_scal_mf[comp], scal_mf, start_scomp+comp, 0, 1, scal_mf.nGrow());
        }

        // Be careful to pass in comp+1 for fortran indexing
        for (int scomp = start_scomp; scomp < start_scomp + num_comp; ++scomp) {

            int vcomp = scomp - start_scomp;
            int bccomp = start_bccomp + scomp - start_scomp;

#ifdef _OPENMP
#pragma omp parallel
#endif
            for ( MFIter mfi(scal_mf, TilingIfNotGPU()); mfi.isValid(); ++mfi ) {

                // Get the index space of the valid region
                const Box& tileBox = mfi.tilebox();
                const Box& obx = amrex::grow(tileBox, 1);

                Array4<Real> const umac_arr = umac[lev][0].array(mfi);
                Array4<Real> const vmac_arr = umac[lev][1].array(mfi);
                Array4<Real> const wmac_arr = umac[lev][2].array(mfi);

                // make divu 
                if (is_conservative) {
                    MakeDivU(obx, divu.array(mfi), 
                             umac_arr, vmac_arr, wmac_arr, dx);
                }
                          
                if (ppm_type == 0) {
                    // we're going to reuse Ip here as slopex and Im as slopey
                    // as they have the correct number of ghost zones

                    // x-direction
                    Slopex(obx, vec_scal_mf[vcomp].array(mfi), 
                           Ip.array(mfi), 
                           domainBox, bcs, 
                           1,bccomp);

                    // y-direction
                    Slopey(obx, vec_scal_mf[vcomp].array(mfi), 
                           Im.array(mfi), 
                           domainBox, bcs, 
                           1,bccomp);

                    // z-direction
                    Slopez(obx, vec_scal_mf[vcomp].array(mfi), 
                           slopez.array(mfi), 
                           domainBox, bcs, 
                           1,bccomp);

                } else {

                    PPM(obx, state[lev].array(mfi), 
                        umac_arr, vmac_arr, wmac_arr,
                        Ip.array(mfi), Im.array(mfi), 
                        domainBox, bcs, dx, 
                        true, scomp, bccomp);

                    if (ppm_trace_forces == 1) {

                        PPM(obx, force[lev].array(mfi), 
                            umac_arr, vmac_arr, wmac_arr,
                            Ipf.array(mfi), Imf.array(mfi), 
                            domainBox, bcs, dx, 
                            true, scomp, bccomp);
                    }
                }
            }

#ifdef _OPENMP
#pragma omp parallel
#endif
            for ( MFIter mfi(scal_mf, TilingIfNotGPU()); mfi.isValid(); ++mfi ) {

                Array4<Real> const scal_arr = state[lev].array(mfi);

                Array4<Real> const umac_arr = umac[lev][0].array(mfi);
                Array4<Real> const vmac_arr = umac[lev][1].array(mfi);
                Array4<Real> const wmac_arr = umac[lev][2].array(mfi);

                Array4<Real> const slx_arr = slx.array(mfi);
                Array4<Real> const srx_arr = srx.array(mfi);
                Array4<Real> const sly_arr = sly.array(mfi);
                Array4<Real> const sry_arr = sry.array(mfi);
                Array4<Real> const slz_arr = slz.array(mfi);
                Array4<Real> const srz_arr = srz.array(mfi);

                Array4<Real> const simhx_arr = simhx.array(mfi);
                Array4<Real> const simhy_arr = simhy.array(mfi);
                Array4<Real> const simhz_arr = simhz.array(mfi);

                // Create s_{\i-\half\e_x}^x, etc.

                MakeEdgeScalPredictor(mfi, slx_arr, srx_arr,
                                      sly_arr, sry_arr,
                                      slz_arr, srz_arr,
                                      scal_arr, 
                                      Ip.array(mfi), Im.array(mfi), 
                                      slopez.array(mfi),
                                      umac_arr, vmac_arr, wmac_arr,
                                      simhx_arr, simhy_arr, simhz_arr,
                                      domainBox, bcs, dx,
                                      scomp, bccomp, is_vel);

                Array4<Real> const simhxy_arr = simhxy.array(mfi);
                Array4<Real> const simhxz_arr = simhxz.array(mfi);
                Array4<Real> const simhyx_arr = simhyx.array(mfi);
                Array4<Real> const simhyz_arr = simhyz.array(mfi);
                Array4<Real> const simhzx_arr = simhzx.array(mfi);
                Array4<Real> const simhzy_arr = simhzy.array(mfi);

                // Create transverse terms, s_{\i-\half\e_x}^{x|y}, etc.

                MakeEdgeScalTransverse(mfi, slx_arr, srx_arr,
                                       sly_arr, sry_arr,
                                       slz_arr, srz_arr,
                                       scal_arr, divu.array(mfi),
                                       umac_arr, vmac_arr, wmac_arr,
                                       simhx_arr, simhy_arr, simhz_arr,
                                       simhxy_arr, simhxz_arr, simhyx_arr,
                                       simhyz_arr, simhzx_arr, simhzy_arr,
                                       domainBox, bcs, dx,
                                       scomp, bccomp, 
                                       is_vel, is_conservative);

                Array4<Real> const sedgex_arr = sedge[lev][0].array(mfi);
                Array4<Real> const sedgey_arr = sedge[lev][1].array(mfi);
                Array4<Real> const sedgez_arr = sedge[lev][2].array(mfi);

                // Create sedgelx, etc.

                MakeEdgeScalEdges(mfi, slx_arr, srx_arr,
                                  sly_arr, sry_arr,
                                  slz_arr, srz_arr, scal_arr, 
                                  sedgex_arr, sedgey_arr, sedgez_arr,
                                  force[lev].array(mfi),
                                  umac_arr, vmac_arr, wmac_arr,
                                  Ipf.array(mfi), Imf.array(mfi),
                                  simhxy_arr, simhxz_arr, simhyx_arr,
                                  simhyz_arr, simhzx_arr, simhzy_arr,
                                  domainBox, bcs, dx,
                                  scomp, bccomp, 
                                  is_vel, is_conservative);
            } // end MFIter loop
        } // end loop over components
#endif
    } // end loop over levels

    // We use edge_restriction for the output velocity if is_vel == 1
    // we do not use edge_restriction for scalars because instead we will use
    // reflux on the fluxes in make_flux.
    if (is_vel == 1) {
        if (reflux_type == 1 || reflux_type == 2) {
            AverageDownFaces(sedge);
        }
    }
}

#if (AMREX_SPACEDIM == 2)

void Maestro::MakeEdgeScalPredictor(const MFIter& mfi,
                                    Array4<Real> const slx,
                                    Array4<Real> const srx,
                                    Array4<Real> const sly,
                                    Array4<Real> const sry,
                                    Array4<Real> const s,
                                    Array4<Real> const Ip,
                                    Array4<Real> const Im, 
                                    Array4<Real> const umac,
                                    Array4<Real> const vmac,
                                    Array4<Real> const simhx,
                                    Array4<Real> const simhy,
                                    const Box& domainBox,
                                    const Vector<BCRec>& bcs,
                                    const Real* dx,
                                    int comp, int bccomp, bool is_vel)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeEdgeScalPredictor()",MakeEdgeScalPredictor);

    ///////////////////////////////////////
    // Create s_{\i-\half\e_x}^x, etc.
    ///////////////////////////////////////

    Real ppm_type_local = ppm_type;
    Real hx = dx[0];
    Real hy = dx[1];

    Real dt2 = 0.5 * dt;

    // Get the index space of the valid region
    const Box& tileBox = mfi.tilebox();
    const Box& obx = amrex::grow(tileBox, 1);
    const Box& mxbx = amrex::growLo(obx, 0, -1);
    const Box& mybx = amrex::growLo(obx, 1, -1);

    const Real rel_eps = c_rel_eps;

    // loop over appropriate x-faces
    int ilo = domainBox.loVect()[0];
    int ihi = domainBox.hiVect()[0];
    int bclo = bcs[bccomp].lo()[0];
    int bchi = bcs[bccomp].hi()[0];
    AMREX_PARALLEL_FOR_3D(mxbx, i, j, k, 
    {
        if (ppm_type_local == 0) {
            // make slx, srx with 1D extrapolation
            slx(i,j,k) = s(i-1,j,k,comp) + (0.5 - dt2*umac(i,j,k)/hx)*Ip(i-1,j,k,0);
            srx(i,j,k) = s(i,j,k,comp) - (0.5 + dt2*umac(i,j,k)/hx)*Ip(i,j,k,0);
        } else if (ppm_type_local == 1 || ppm_type_local == 2) {
            // make slx, srx with 1D extrapolation
            slx(i,j,k) = Ip(i-1,j,k,0);
            srx(i,j,k) = Im(i,j,k,0);
        }

        // impose lo side bc's
        if (i == ilo) {
            if (bclo == EXT_DIR) {
                slx(i,j,k) = s(i-1,j,k,comp);
                srx(i,j,k) = s(i-1,j,k,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    srx(i,j,k) = min(srx(i,j,k),0.0);
                }
                slx(i,j,k) = srx(i,j,k);
            } else if (bclo == REFLECT_EVEN) {
                slx(i,j,k) = srx(i,j,k);
            } else if (bclo == REFLECT_ODD) {
                slx(i,j,k) = 0.0;
                srx(i,j,k) = 0.0;
            }

        // impose hi side bc's
        } else if (i == ihi+1) {
            if (bchi == EXT_DIR) {
                slx(i,j,k) = s(i,j,k,comp);
                srx(i,j,k) = s(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    slx(i,j,k) = max(slx(i,j,k),0.0);
                }
                srx(i,j,k) = slx(i,j,k);
            } else if (bchi == REFLECT_EVEN) {
                srx(i,j,k) = slx(i,j,k);
            } else if (bchi == REFLECT_ODD) {
                slx(i,j,k) = 0.0;
                srx(i,j,k) = 0.0;
            }
        }

        // make simhx by solving Riemann problem
        simhx(i,j,k) = (umac(i,j,k) > 0.0) ? slx(i,j,k) : srx(i,j,k);
        simhx(i,j,k) = (fabs(umac(i,j,k)) > rel_eps) ? 
            simhx(i,j,k) : 0.5*(slx(i,j,k)+srx(i,j,k));

    });

    // loop over appropriate y-faces
    int jlo = domainBox.loVect()[1];
    int jhi = domainBox.hiVect()[1];
    bclo = bcs[bccomp].lo()[1];
    bchi = bcs[bccomp].hi()[1];
    AMREX_PARALLEL_FOR_3D(mybx, i, j, k, 
    {

        if (ppm_type_local == 0) {
            // make sly, sry with 1D extrapolation
            sly(i,j,k) = s(i,j-1,k,comp) + (0.5 - dt2*vmac(i,j,k)/hy)*Im(i,j-1,k,0);
            sry(i,j,k) = s(i,j,k,comp) - (0.5 + dt2*vmac(i,j,k)/hy)*Im(i,j,k,0);
        } else if (ppm_type_local == 1 || ppm_type_local == 2) {
            // make sly, sry with 1D extrapolation
            sly(i,j,k) = Ip(i,j-1,k,1);
            sry(i,j,k) = Im(i,j,k,1);
        }

        // impose lo side bc's
        if (j == jlo) {
            if (bclo == EXT_DIR) {
                sly(i,j,k) = s(i,j-1,k,comp);
                sry(i,j,k) = s(i,j-1,k,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sry(i,j,k) = min(sry(i,j,k),0.0);
                }
                sly(i,j,k) = sry(i,j,k);
            } else if (bclo == REFLECT_EVEN) {
                sly(i,j,k) = sry(i,j,k);
            } else if (bclo == REFLECT_ODD) {
                sly(i,j,k) = 0.0;
                sry(i,j,k) = 0.0;
            }

        // impose hi side bc's
        } else if (j == jhi+1) {
            if (bchi == EXT_DIR) {
                sly(i,j,k) = s(i,j,k,comp);
                sry(i,j,k) = s(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sly(i,j,k) = max(sly(i,j,k),0.0);
                }
                sry(i,j,k) = sly(i,j,k);
            } else if (bchi == REFLECT_EVEN) {
                sry(i,j,k) = sly(i,j,k);
            } else if (bchi == REFLECT_ODD) {
                sly(i,j,k) = 0.0;
                sry(i,j,k) = 0.0;
            }
        }

        // make simhy by solving Riemann problem
        simhy(i,j,k) = (vmac(i,j,k) > 0.0) ? sly(i,j,k) : sry(i,j,k);
        simhy(i,j,k) = (fabs(vmac(i,j,k)) > rel_eps) ? 
            simhy(i,j,k) : 0.5*(sly(i,j,k)+sry(i,j,k));
    });
}

void Maestro::MakeEdgeScalEdges(const MFIter& mfi,
                            Array4<Real> const slx,
                            Array4<Real> const srx,
                            Array4<Real> const sly,
                            Array4<Real> const sry,
                            Array4<Real> const s,
                            Array4<Real> const sedgex,
                            Array4<Real> const sedgey,
                            Array4<Real> const force,
                            Array4<Real> const umac,
                            Array4<Real> const vmac,
                            Array4<Real> const Ipf,
                            Array4<Real> const Imf,
                            Array4<Real> const simhx,
                            Array4<Real> const simhy,
                            const Box& domainBox,
                            const Vector<BCRec>& bcs,
                            const Real* dx,
                            int comp, int bccomp, 
                            bool is_vel, bool is_conservative) 
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeEdgeScalEdges()",MakeEdgeScalEdges);

    ///////////////////////////////////////////////
    // Create sedgelx, etc.
    ///////////////////////////////////////////////

    int ppm_trace_forces_local = ppm_trace_forces;

    Real dt2 = 0.5 * dt;
    Real dt4 = 0.25 * dt;

    Real hx = dx[0];
    Real hy = dx[1];

    int ilo = domainBox.loVect()[0];
    int ihi = domainBox.hiVect()[0];
    int jlo = domainBox.loVect()[1];
    int jhi = domainBox.hiVect()[1];

    const Box& xbx = mfi.nodaltilebox(0);
    const Box& ybx = mfi.nodaltilebox(1);

    const Real rel_eps = c_rel_eps;

    // x-direction
    int bclo = bcs[bccomp].lo()[0];
    int bchi = bcs[bccomp].hi()[0];
    AMREX_PARALLEL_FOR_3D(xbx, i, j, k, 
    {
        Real sedgelx = 0.0;
        Real sedgerx = 0.0;

        Real fl = (ppm_trace_forces_local == 0) ? 
            force(i-1,j,k,comp) : Ipf(i-1,j,k,0);
        Real fr = (ppm_trace_forces_local == 0) ? 
            force(i,j,k,comp) : Imf(i,j,k,0);

        if (is_conservative) {
            sedgelx = slx(i,j,k)
                - (dt2/hy)*(simhy(i-1,j+1,k)*vmac(i-1,j+1,k) 
                - simhy(i-1,j,k)*vmac(i-1,j,k))
                - (dt2/hx)*s(i-1,j,k,comp)*(umac(i  ,j,k)-umac(i-1,j,k))
                + dt2*fl;
            sedgerx = srx(i,j,k)
                - (dt2/hy)*(simhy(i  ,j+1,k)*vmac(i  ,j+1,k) 
                - simhy(i  ,j,k)*vmac(i  ,j,k))
                - (dt2/hx)*s(i  ,j,k,comp)*(umac(i+1,j,k)-umac(i  ,j,k))
                + dt2*fr;
        } else {
            sedgelx = slx(i,j,k)
                - (dt4/hy)*(vmac(i-1,j+1,k)+vmac(i-1,j,k))*
                (simhy(i-1,j+1,k)-simhy(i-1,j,k))
                + dt2*fl;
            sedgerx = srx(i,j,k)
                - (dt4/hy)*(vmac(i  ,j+1,k)+vmac(i  ,j,k))*
                (simhy(i  ,j+1,k)-simhy(i  ,j,k))
                + dt2*fr;
        }

        // make sedgex by solving Riemann problem
        // boundary conditions enforced outside of i,j loop
        sedgex(i,j,k,comp) = (umac(i,j,k) > 0.0) ? sedgelx : sedgerx;
        sedgex(i,j,k,comp) = (fabs(umac(i,j,k)) > rel_eps) ? 
            sedgex(i,j,k,comp) : 0.5*(sedgelx+sedgerx);

        // impose lo side bc's
        if (i == ilo) {
            if (bclo == EXT_DIR) {
                sedgex(i,j,k,comp) = s(i-1,j,k,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    sedgex(i,j,k,comp) = min(sedgerx,0.0);
                } else {
                    sedgex(i,j,k,comp) = sedgerx;
                }
            } else if (bclo == REFLECT_EVEN) {
                sedgex(i,j,k,comp) = sedgerx;
            } else if (bclo == REFLECT_ODD) {
                sedgex(i,j,k,comp) = 0.0;
            }

        // impose hi side bc's
        } else if (i == ihi+1) {
            if (bchi == EXT_DIR) {
                sedgex(i,j,k,comp) = s(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    sedgex(i,j,k,comp) = max(sedgelx,0.0);
                } else {
                    sedgex(i,j,k,comp) = sedgelx;
                }
            } else if (bchi == REFLECT_EVEN) {
                sedgex(i,j,k,comp) = sedgelx;
            } else if (bchi == REFLECT_ODD) {
                sedgex(i,j,k,comp) = 0.0;
            }
        }
    });

    // y-direction
    bclo = bcs[bccomp].lo()[1];
    bchi = bcs[bccomp].hi()[1];
    AMREX_PARALLEL_FOR_3D(ybx, i, j, k, 
    {
        Real sedgely = 0.0;
        Real sedgery = 0.0;

        Real fl = (ppm_trace_forces_local == 0) ? 
            force(i,j-1,k,comp) : Ipf(i,j-1,k,1);
        Real fr = (ppm_trace_forces_local == 0) ? 
            force(i,j,k,comp) : Imf(i,j,k,1);

        // make sedgely, sedgery
        if (is_conservative) {
            sedgely = sly(i,j,k)
                   - (dt2/hx)*(simhx(i+1,j-1,k)*umac(i+1,j-1,k) - simhx(i,j-1,k)*umac(i,j-1,k))
                   - (dt2/hy)*s(i,j-1,k,comp)*(vmac(i,j,k)-vmac(i,j-1,k))
                   + dt2*fl;
            sedgery = sry(i,j,k)
                - (dt2/hx)*(simhx(i+1,j,k)*umac(i+1,j,k) - simhx(i,j,k)*umac(i,j,k))
                - (dt2/hy)*s(i,j,k,comp)*(vmac(i,j+1,k)-vmac(i,j,k))
                + dt2*fr;
        } else {
            sedgely = sly(i,j,k)
                - (dt4/hx)*(umac(i+1,j-1,k)+umac(i,j-1,k))*(simhx(i+1,j-1,k)-simhx(i,j-1,k))
                + dt2*fl;
            sedgery = sry(i,j,k)
                - (dt4/hx)*(umac(i+1,j,k)+umac(i,j,k))*(simhx(i+1,j,k)-simhx(i,j,k))
                + dt2*fr;
        }

        // make sedgey by solving Riemann problem
        // boundary conditions enforced outside of i,j loop
        sedgey(i,j,k,comp) = (vmac(i,j,k) > 0.0) ? sedgely : sedgery;
        sedgey(i,j,k,comp) = (fabs(vmac(i,j,k)) > rel_eps) ? 
            sedgey(i,j,k,comp): 0.5*(sedgely+sedgery);

        // impose lo side bc's
        if (j == jlo) {
            if (bclo == EXT_DIR) {
                sedgey(i,j,k,comp) = s(i,j-1,k,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sedgey(i,j,k,comp) = min(sedgery,0.0);
                } else {
                    sedgey(i,j,k,comp) = sedgery;
                }
            } else if (bclo == REFLECT_EVEN) {
                sedgey(i,j,k,comp) = sedgery;
            } else if (bclo == REFLECT_ODD) {
                sedgey(i,j,k,comp) = 0.0;
            }

        // impose hi side bc's
        } else if (j == jhi+1) {
            if (bchi == EXT_DIR) {
                sedgey(i,j,k,comp) = s(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sedgey(i,j,k,comp) = max(sedgely,0.0);
                } else {
                    sedgey(i,j,k,comp) = sedgely;
                }
            } else if (bchi == REFLECT_EVEN) {
                sedgey(i,j,k,comp) = sedgely;
            } else if (bchi == REFLECT_ODD) {
                sedgey(i,j,k,comp) = 0.0;
            }
        }
    });
}

#else

void Maestro::MakeDivU(const Box& bx, 
                       Array4<Real> const divu,
                       Array4<Real> const umac,
                       Array4<Real> const vmac,
                       Array4<Real> const wmac,
                       const Real* dx) 
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeDivU()",MakeDivU);

    Real dx_local = dx[0];

    AMREX_PARALLEL_FOR_3D(bx, i, j, k, 
    {
        divu(i,j,k) = umac(i+1,j,k) - umac(i,j,k) +
                      vmac(i,j+1,k) - vmac(i,j,k) +
                      wmac(i,j,k+1) - wmac(i,j,k);
        divu(i,j,k) /= dx_local;
    });
}

void Maestro::MakeEdgeScalPredictor(const MFIter& mfi,
                                    Array4<Real> const slx,
                                    Array4<Real> const srx,
                                    Array4<Real> const sly,
                                    Array4<Real> const sry,
                                    Array4<Real> const slz,
                                    Array4<Real> const srz,
                                    Array4<Real> const scal,
                                    Array4<Real> const Ip,
                                    Array4<Real> const Im, 
                                    Array4<Real> const slopez, 
                                    Array4<Real> const umac,
                                    Array4<Real> const vmac,
                                    Array4<Real> const wmac,
                                    Array4<Real> const simhx,
                                    Array4<Real> const simhy,
                                    Array4<Real> const simhz,
                                    const Box& domainBox,
                                    const Vector<BCRec>& bcs,
                                    const Real* dx,
                                    int comp, int bccomp, bool is_vel)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeEdgeScalPredictor()",MakeEdgeScalPredictor);

    ///////////////////////////////////////
    // Create s_{\i-\half\e_x}^x, etc.
    ///////////////////////////////////////

    Real ppm_type_local = ppm_type;
    Real hx = dx[0];
    Real hy = dx[1];
    Real hz = dx[2];

    // Get the index space of the valid region
    const Box& tileBox = mfi.tilebox();
    const Box& obx = amrex::grow(tileBox, 1);
    const Box& mxbx = amrex::growLo(obx, 0, -1);
    const Box& mybx = amrex::growLo(obx, 1, -1);
    const Box& mzbx = amrex::growLo(obx, 2, -1);

    // loop over appropriate x-faces
    int ilo = domainBox.loVect()[0];
    int ihi = domainBox.hiVect()[0];
    int bclo = bcs[bccomp].lo()[0];
    int bchi = bcs[bccomp].hi()[0];
    AMREX_PARALLEL_FOR_3D(mxbx, i, j, k, 
    {
        if (ppm_type_local == 0) {
            slx(i,j,k) = scal(i-1,j,k,comp) + 
                0.5 * (1.0 - dt * umac(i,j,k) / hx) * Ip(i-1,j,k,0);
            srx(i,j,k) = scal(i,j,k,comp) - 
                0.5 * (1.0 + dt * umac(i,j,k) / hx) * Ip(i,j,k,0);
        } else if (ppm_type_local == 1 || ppm_type_local == 2) {
            slx(i,j,k) = Ip(i-1,j,k,0);
            srx(i,j,k) = Im(i,j,k,0);
        }

        // impose lo side bc's
        if (i == ilo) {
            if (bclo == EXT_DIR) {
                slx(i,j,k) = scal(i-1,j,k,comp);
                srx(i,j,k) = scal(i-1,j,k,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    srx(i,j,k) = min(srx(i,j,k), 0.0);
                }
                slx(i,j,k) = srx(i,j,k);
            } else if (bclo == REFLECT_EVEN) {
                slx(i,j,k) = srx(i,j,k);
            } else if (bclo == REFLECT_ODD) {
                slx(i,j,k) = 0.0;
                srx(i,j,k) = 0.0;
            }

        // impose hi side bc's
        } else if (i == ihi+1) {
            if (bchi == EXT_DIR) {
                slx(i,j,k) = scal(i,j,k,comp);
                srx(i,j,k) = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    slx(i,j,k) = max(slx(i,j,k), 0.0);
                }
                srx(i,j,k) = slx(i,j,k);
            } else if (bchi == REFLECT_EVEN) {
                srx(i,j,k) = slx(i,j,k);
            } else if (bchi == REFLECT_ODD) {
                slx(i,j,k) = 0.0;
                srx(i,j,k) = 0.0;
            }
        }

        // make simhx by solving Riemann problem
        simhx(i,j,k) = (umac(i,j,k) > 0.0) ? 
            slx(i,j,k) : srx(i,j,k);
        simhx(i,j,k) = (fabs(umac(i,j,k)) > 0.0) ? 
            simhx(i,j,k) : 0.5 * (slx(i,j,k) + srx(i,j,k));
    });

    // loop over appropriate y-faces
    int jlo = domainBox.loVect()[1];
    int jhi = domainBox.hiVect()[1];
    bclo = bcs[bccomp].lo()[1];
    bchi = bcs[bccomp].hi()[1];
    AMREX_PARALLEL_FOR_3D(mybx, i, j, k, 
    {
        if (ppm_type_local == 0) {
            sly(i,j,k) = scal(i,j-1,k,comp) + 
                0.5 * (1.0 - dt * vmac(i,j,k) / hy) * Im(i,j-1,k,0);
            sry(i,j,k) = scal(i,j,k,comp) - 
                0.5 * (1.0 + dt * vmac(i,j,k) / hy) * Im(i,j,k,0);
        } else if (ppm_type_local == 1 || ppm_type_local == 2) {
            sly(i,j,k) = Ip(i,j-1,k,1);
            sry(i,j,k) = Im(i,j,k,1);
        }

        // impose lo side bc's
        if (j == jlo) {
            if (bclo == EXT_DIR) {
                sly(i,j,k) = scal(i,j-1,k,comp);
                sry(i,j,k) = scal(i,j-1,k,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sry(i,j,k) = min(sry(i,j,k), 0.0);
                }
                sly(i,j,k) = sry(i,j,k);
            } else if (bclo == REFLECT_EVEN) {
                sly(i,j,k) = sry(i,j,k);
            } else if (bclo == REFLECT_ODD) {
                sly(i,j,k) = 0.0;
                sry(i,j,k) = 0.0;
            }
        // impose hi side bc's
        } else if (j == jhi+1) {
            if (bchi == EXT_DIR) {
                sly(i,j,k) = scal(i,j,k,comp);
                sry(i,j,k) = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sly(i,j,k) = max(sly(i,j,k), 0.0);
                }
                sry(i,j,k) = sly(i,j,k);
            } else if (bchi == REFLECT_EVEN) {
                sry(i,j,k) = sly(i,j,k);
            } else if (bchi == REFLECT_ODD) {
                sly(i,j,k) = 0.0;
                sry(i,j,k) = 0.0;
            }
        }

        // make simhy by solving Riemann problem
        simhy(i,j,k) = (vmac(i,j,k) > 0.0) ? 
            sly(i,j,k) : sry(i,j,k);
        simhy(i,j,k) = (fabs(vmac(i,j,k)) > 0.0) ? 
            simhy(i,j,k) : 0.5 * (sly(i,j,k) + sry(i,j,k));

    });

    // loop over appropriate z-faces
    int klo = domainBox.loVect()[2];
    int khi = domainBox.hiVect()[2];
    bclo = bcs[bccomp].lo()[2];
    bchi = bcs[bccomp].hi()[2];
    AMREX_PARALLEL_FOR_3D(mzbx, i, j, k, 
    {
        if (ppm_type_local == 0) {
            slz(i,j,k) = scal(i,j,k-1,comp) + 
                0.5 * (1.0 - dt * wmac(i,j,k) / hz) * slopez(i,j,k-1);
            srz(i,j,k) = scal(i,j,k,comp) - 
                0.5 * (1.0 + dt * wmac(i,j,k) / hz) * slopez(i,j,k);
        } else if (ppm_type_local == 1 || ppm_type_local == 2) {
            slz(i,j,k) = Ip(i,j,k-1,2);
            srz(i,j,k) = Im(i,j,k,2);
        }

        // impose lo side bc's
        if (k == klo) {
            if (bclo == EXT_DIR) {
                slz(i,j,k) = scal(i,j,k-1,comp);
                srz(i,j,k) = scal(i,j,k-1,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 2) {
                    srz(i,j,k) = min(srz(i,j,k), 0.0);
                }
                slz(i,j,k) = srz(i,j,k);
            } else if (bclo == REFLECT_EVEN) {
                slz(i,j,k) = srz(i,j,k);
            } else if (bclo == REFLECT_ODD) {
                slz(i,j,k) = 0.0;
                srz(i,j,k) = 0.0;
            }
        // impose hi side bc's
        } else if (k == khi+1) {
            if (bchi == EXT_DIR) {
                slz(i,j,k) = scal(i,j,k,comp);
                srz(i,j,k) = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 2) {
                    slz(i,j,k) = max(slz(i,j,k), 0.0);
                }
                srz(i,j,k) = slz(i,j,k);
            } else if (bchi == REFLECT_EVEN) {
                srz(i,j,k) = slz(i,j,k);
            } else if (bchi == REFLECT_ODD) {
                slz(i,j,k) = 0.0;
                srz(i,j,k) = 0.0;
            }
        }

        simhz(i,j,k) = (wmac(i,j,k) > 0.0) ? 
            slz(i,j,k) : srz(i,j,k);
        simhz(i,j,k) = (fabs(wmac(i,j,k)) > 0.0) ?
            simhz(i,j,k) : 0.5 * (slz(i,j,k) + srz(i,j,k));
    });

}

void Maestro::MakeEdgeScalTransverse(const MFIter& mfi,
                                    Array4<Real> const slx,
                                    Array4<Real> const srx,
                                    Array4<Real> const sly,
                                    Array4<Real> const sry,
                                    Array4<Real> const slz,
                                    Array4<Real> const srz,
                                    Array4<Real> const scal,
                                    Array4<Real> const divu,
                                    Array4<Real> const umac,
                                    Array4<Real> const vmac,
                                    Array4<Real> const wmac,
                                    Array4<Real> const simhx,
                                    Array4<Real> const simhy,
                                    Array4<Real> const simhz,
                                    Array4<Real> const simhxy,
                                    Array4<Real> const simhxz,
                                    Array4<Real> const simhyx,
                                    Array4<Real> const simhyz,
                                    Array4<Real> const simhzx,
                                    Array4<Real> const simhzy,
                                    const Box& domainBox,
                                    const Vector<BCRec>& bcs,
                                    const Real* dx,
                                    int comp, int bccomp, 
                                    bool is_vel, bool is_conservative)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeEdgeScalTransverse()",MakeEdgeScalTransverse);

    ////////////////////////////////////////////////////////
    // Create transverse terms, s_{\i-\half\e_x}^{x|y}, etc.
    ////////////////////////////////////////////////////////

    Real dt3 = dt / 3.0;
    Real dt6 = dt / 6.0;

    Real hx = dx[0];
    Real hy = dx[1];
    Real hz = dx[2];

    int ilo = domainBox.loVect()[0];
    int ihi = domainBox.hiVect()[0];
    int jlo = domainBox.loVect()[1];
    int jhi = domainBox.hiVect()[1];
    int klo = domainBox.loVect()[2];
    int khi = domainBox.hiVect()[2];

    const Real rel_eps = c_rel_eps;

    // simhxy
    Box imhbox = amrex::grow(mfi.tilebox(), 2, 1);
    imhbox = amrex::growHi(imhbox, 0, 1);
    // Box imhbox = mfi.grownnodaltilebox(0, amrex::IntVect(0,0,1)); 
    int bclo = bcs[bccomp].lo()[0];
    int bchi = bcs[bccomp].hi()[0];
    AMREX_PARALLEL_FOR_3D(imhbox, i, j, k, 
    {
        Real slxy = 0.0;
        Real srxy = 0.0;
        
        // loop over appropriate xy faces
        if (is_conservative) {
            // make slxy, srxy by updating 1D extrapolation
            slxy = slx(i,j,k) 
                - (dt3/hy) * (simhy(i-1,j+1,k)*vmac(i-1,j+1,k) 
                - simhy(i-1,j,k)*vmac(i-1,j,k)) 
                - dt3*scal(i-1,j,k,comp)*divu(i-1,j,k) 
                + (dt3/hy)*scal(i-1,j,k,comp)*
                (vmac(i-1,j+1,k)-vmac(i-1,j,k));
            srxy = srx(i,j,k) 
                - (dt3/hy)*(simhy(i,j+1,k)*vmac(i,j+1,k)
                - simhy(i,j,k)*vmac(i,j,k)) 
                - dt3*scal(i,j,k,comp)*divu(i,j,k) 
                + (dt3/hy)*scal(i,j,k,comp)*
                (vmac(i,j+1,k)-vmac(i,j,k));
        } else {
            // make slxy, srxy by updating 1D extrapolation
            slxy = slx(i,j,k) 
                - (dt6/hy)*(vmac(i-1,j+1,k)+vmac(i-1,j,k)) 
                *(simhy(i-1,j+1,k)-simhy(i-1,j,k));
            srxy = srx(i,j,k) 
                - (dt6/hy)*(vmac(i,j+1,k)+vmac(i,j,k))
                *(simhy(i,j+1,k)-simhy(i,j,k));
        }

        // impose lo side bc's
        if (i == ilo) {
            if (bclo == EXT_DIR) {
                slxy = scal(i-1,j,k,comp);
                srxy = scal(i-1,j,k,comp);
            } else if (bclo ==  FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    srxy = min(srxy,0.0);
                }
                slxy = srxy;
            } else if (bclo == REFLECT_EVEN) {
                slxy = srxy;
            } else if (bclo ==  REFLECT_ODD) {
                slxy = 0.0;
                srxy = 0.0;
            }

        // impose hi side bc's
        } else if (i == ihi+1) {
            if (bchi == EXT_DIR) {
                slxy = scal(i,j,k,comp);
                srxy = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    slxy = max(slxy,0.0);
                }
                srxy = slxy;
            } else if (bchi == REFLECT_EVEN) {
                srxy = slxy;
            } else if (bchi == REFLECT_ODD) {
                slxy = 0.0;
                srxy = 0.0;
            }
        }

        // make simhxy by solving Riemann problem
        simhxy(i,j,k) = (umac(i,j,k) > 0.0) ?
            slxy : srxy;
        simhxy(i,j,k) = (fabs(umac(i,j,k)) > rel_eps) ?
            simhxy(i,j,k) : 0.5 * (slxy + srxy);

    });

    // simhxz
    imhbox = amrex::grow(mfi.tilebox(), 1, 1);
    imhbox = amrex::growHi(imhbox, 0, 1);
    // imhbox = mfi.grownnodaltilebox(0, amrex::IntVect(0,1,0));

    AMREX_PARALLEL_FOR_3D(imhbox, i, j, k, 
    {
        Real slxz = 0.0;
        Real srxz = 0.0;
        // loop over appropriate xz faces
        if (is_conservative) {
            // make slxz, srxz by updating 1D extrapolation
            slxz = slx(i,j,k) 
                - (dt3/hz) * (simhz(i-1,j,k+1)*wmac(i-1,j,k+1) 
                - simhz(i-1,j,k)*wmac(i-1,j,k)) 
                - dt3*scal(i-1,j,k,comp)*divu(i-1,j,k) 
                + (dt3/hz)*scal(i-1,j,k,comp)*
                (wmac(i-1,j,k+1)-wmac(i-1,j,k));
            srxz = srx(i,j,k) 
                - (dt3/hz)*(simhz(i,j,k+1)*wmac(i,j,k+1)
                - simhz(i,j,k)*wmac(i,j,k)) 
                - dt3*scal(i,j,k,comp)*divu(i,j,k) 
                + (dt3/hz)*scal(i,j,k,comp)*
                (wmac(i,j,k+1)-wmac(i,j,k));
        } else {
            // make slxz, srxz by updating 1D extrapolation
            slxz = slx(i,j,k) 
                - (dt6/hz)*(wmac(i-1,j,k+1)+wmac(i-1,j,k)) 
                *(simhz(i-1,j,k+1)-simhz(i-1,j,k));
            srxz = srx(i,j,k) 
                - (dt6/hz)*(wmac(i,j,k+1)+wmac(i,j,k)) 
                *(simhz(i,j,k+1)-simhz(i,j,k));
        }

        // impose lo side bc's
        if (i == ilo) {
            if (bclo == EXT_DIR) {
                slxz = scal(i-1,j,k,comp);
                srxz = scal(i-1,j,k,comp);
            } else if (bclo ==  FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    srxz = min(srxz,0.0);
                }
                slxz = srxz;
            } else if (bclo == REFLECT_EVEN) {
                slxz = srxz;
            } else if (bclo ==  REFLECT_ODD) {
                slxz = 0.0;
                srxz = 0.0;
            }

        // impose hi side bc's
        } else if (i == ihi+1) {
            if (bchi == EXT_DIR) {
                slxz = scal(i,j,k,comp);
                srxz = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    slxz = max(slxz,0.0);
                }
                srxz = slxz;
            } else if (bchi == REFLECT_EVEN) {
                srxz = slxz;
            } else if (bchi == REFLECT_ODD) {
                slxz = 0.0;
                srxz = 0.0;
            }
        }

        // make simhxy by solving Riemann problem
        simhxz(i,j,k) = (umac(i,j,k) > 0.0) ?
            slxz : srxz;
        simhxz(i,j,k) = (fabs(umac(i,j,k)) > rel_eps) ?
            simhxz(i,j,k) : 0.5 * (slxz + srxz);

    });

    // simhyx
    // imhbox = mfi.grownnodaltilebox(1, amrex::IntVect(0,0,1));
    imhbox = amrex::grow(mfi.tilebox(), 2, 1);
    imhbox = amrex::growHi(imhbox, 1, 1);

    bclo = bcs[bccomp].lo()[1];
    bchi = bcs[bccomp].hi()[1];

    AMREX_PARALLEL_FOR_3D(imhbox, i, j, k, 
    {
        Real slyx = 0.0;
        Real sryx = 0.0;
        // loop over appropriate yx faces
        if (is_conservative) {
            // make slyx, sryx by updating 1D extrapolation
            slyx = sly(i,j,k) 
                - (dt3/hx) * (simhx(i+1,j-1,k)*umac(i+1,j-1,k) 
                - simhx(i,j-1,k)*umac(i,j-1,k)) 
                - dt3*scal(i,j-1,k,comp)*divu(i,j-1,k) 
                + (dt3/hx)*scal(i,j-1,k,comp)*
                (umac(i+1,j-1,k)-umac(i,j-1,k));
            sryx = sry(i,j,k) 
                - (dt3/hx)*(simhx(i+1,j,k)*umac(i+1,j,k)
                - simhx(i,j,k)*umac(i,j,k)) 
                - dt3*scal(i,j,k,comp)*divu(i,j,k) 
                + (dt3/hx)*scal(i,j,k,comp)*
                (umac(i+1,j,k)-umac(i,j,k));
        } else {
            // make slyx, sryx by updating 1D extrapolation
            slyx = sly(i,j,k) 
                - (dt6/hx)*(umac(i+1,j-1,k)+umac(i,j-1,k)) 
                *(simhx(i+1,j-1,k)-simhx(i,j-1,k));
            sryx = sry(i,j,k) 
                - (dt6/hx)*(umac(i+1,j,k)+umac(i,j,k)) 
                *(simhx(i+1,j,k)-simhx(i,j,k));
        }

        // impose lo side bc's
        if (j == jlo) {
            if (bclo == EXT_DIR) {
                slyx = scal(i,j-1,k,comp);
                sryx = scal(i,j-1,k,comp);
            } else if (bclo ==  FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sryx = min(sryx,0.0);
                }
                slyx = sryx;
            } else if (bclo == REFLECT_EVEN) {
                slyx = sryx;
            } else if (bclo ==  REFLECT_ODD) {
                slyx = 0.0;
                sryx = 0.0;
            }

        // impose hi side bc's
        } else if (j == jhi+1) {
            if (bchi == EXT_DIR) {
                slyx = scal(i,j,k,comp);
                sryx = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    slyx = max(slyx,0.0);
                }
                sryx = slyx;
            } else if (bchi == REFLECT_EVEN) {
                sryx = slyx;
            } else if (bchi == REFLECT_ODD) {
                slyx = 0.0;
                sryx = 0.0;
            }
        }

        // make simhxy by solving Riemann problem
        simhyx(i,j,k) = (vmac(i,j,k) > 0.0) ?
            slyx : sryx;
        simhyx(i,j,k) = (fabs(vmac(i,j,k)) > rel_eps) ?
            simhyx(i,j,k) : 0.5 * (slyx + sryx);
    });

    // simhyz
    // imhbox = mfi.grownnodaltilebox(1, amrex::IntVect(1,0,0)); 
    imhbox = amrex::grow(mfi.tilebox(), 0, 1);
    imhbox = amrex::growHi(imhbox, 1, 1);

    AMREX_PARALLEL_FOR_3D(imhbox, i, j, k, 
    {
        Real slyz = 0.0;
        Real sryz = 0.0;
        // loop over appropriate yz faces
        if (is_conservative) {
            // make slyz, sryz by updating 1D extrapolation
            slyz = sly(i,j,k) 
                - (dt3/hz) * (simhz(i,j-1,k+1)*wmac(i,j-1,k+1) 
                - simhz(i,j-1,k)*wmac(i,j-1,k)) 
                - dt3*scal(i,j-1,k,comp)*divu(i,j-1,k) 
                + (dt3/hz)*scal(i,j-1,k,comp)*
                (wmac(i,j-1,k+1)-wmac(i,j-1,k));
            sryz = sry(i,j,k) 
                - (dt3/hz)*(simhz(i,j,k+1)*wmac(i,j,k+1)
                - simhz(i,j,k)*wmac(i,j,k)) 
                - dt3*scal(i,j,k,comp)*divu(i,j,k) 
                + (dt3/hz)*scal(i,j,k,comp)*
                (wmac(i,j,k+1)-wmac(i,j,k));
        } else {
            // make slyz, sryz by updating 1D extrapolation
            slyz = sly(i,j,k) 
                - (dt6/hz)*(wmac(i,j-1,k+1)+wmac(i,j-1,k)) 
                *(simhz(i,j-1,k+1)-simhz(i,j-1,k));
            sryz = sry(i,j,k) 
                - (dt6/hz)*(wmac(i,j,k+1)+wmac(i,j,k)) 
                *(simhz(i,j,k+1)-simhz(i,j,k));
        }

        // impose lo side bc's
        if (j == jlo) {
            if (bclo == EXT_DIR) {
                slyz = scal(i,j-1,k,comp);
                sryz = scal(i,j-1,k,comp);
            } else if (bclo ==  FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sryz = min(sryz,0.0);
                }
                slyz = sryz;
            } else if (bclo == REFLECT_EVEN) {
                slyz = sryz;
            } else if (bclo ==  REFLECT_ODD) {
                slyz = 0.0;
                sryz = 0.0;
            }

        // impose hi side bc's
        } else if (j == jhi+1) {
            if (bchi == EXT_DIR) {
                slyz = scal(i,j,k,comp);
                sryz = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    slyz = max(slyz,0.0);
                }
                sryz = slyz;
            } else if (bchi == REFLECT_EVEN) {
                sryz = slyz;
            } else if (bchi == REFLECT_ODD) {
                slyz = 0.0;
                sryz = 0.0;
            }
        }

        // make simhyz by solving Riemann problem
        simhyz(i,j,k) = (vmac(i,j,k) > 0.0) ?
            slyz : sryz;
        simhyz(i,j,k) = (fabs(vmac(i,j,k)) > rel_eps) ?
            simhyz(i,j,k) : 0.5 * (slyz + sryz);
    });

    // simhzx
    // imhbox = mfi.grownnodaltilebox(2, amrex::IntVect(0,1,0));
    imhbox = amrex::grow(mfi.tilebox(), 1, 1);
    imhbox = amrex::growHi(imhbox, 2, 1);

    bclo = bcs[bccomp].lo()[2];
    bchi = bcs[bccomp].hi()[2];

    AMREX_PARALLEL_FOR_3D(imhbox, i, j, k, 
    {
        Real slzx = 0.0;
        Real srzx = 0.0;
        // loop over appropriate zx faces
        if (is_conservative) {
            // make slzx, srzx by updating 1D extrapolation
            slzx = slz(i,j,k) 
                - (dt3/hx) * (simhx(i+1,j,k-1)*umac(i+1,j,k-1) 
                - simhx(i,j,k-1)*umac(i,j,k-1)) 
                - dt3*scal(i,j,k-1,comp)*divu(i,j,k-1) 
                + (dt3/hx)*scal(i,j,k-1,comp)*
                (umac(i+1,j,k-1)-umac(i,j,k-1));
            srzx = srz(i,j,k) 
                - (dt3/hx)*(simhx(i+1,j,k)*umac(i+1,j,k)
                - simhx(i,j,k)*umac(i,j,k)) 
                - dt3*scal(i,j,k,comp)*divu(i,j,k) 
                + (dt3/hx)*scal(i,j,k,comp)*
                (umac(i+1,j,k)-umac(i,j,k));
        } else {
            // make slzx, srzx by updating 1D extrapolation
            slzx = slz(i,j,k) 
                - (dt6/hx)*(umac(i+1,j,k-1)+umac(i,j,k-1)) 
                *(simhx(i+1,j,k-1)-simhx(i,j,k-1));
            srzx = srz(i,j,k) 
                - (dt6/hx)*(umac(i+1,j,k)+umac(i,j,k)) 
                *(simhx(i+1,j,k)-simhx(i,j,k));
        }

        // impose lo side bc's
        if (k == klo) {
            if (bclo == EXT_DIR) {
                slzx = scal(i,j,k-1,comp);
                srzx = scal(i,j,k-1,comp);
            } else if (bclo ==  FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 2) {
                    srzx = min(srzx,0.0);
                }
                slzx = srzx;
            } else if (bclo == REFLECT_EVEN) {
                slzx = srzx;
            } else if (bclo ==  REFLECT_ODD) {
                slzx = 0.0;
                srzx = 0.0;
            }

        // impose hi side bc's
        } else if (k == khi+1) {
            if (bchi == EXT_DIR) {
                slzx = scal(i,j,k,comp);
                srzx = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 2) {
                    slzx = max(slzx,0.0);
                }
                srzx = slzx;
            } else if (bchi == REFLECT_EVEN) {
                srzx = slzx;
            } else if (bchi == REFLECT_ODD) {
                slzx = 0.0;
                srzx = 0.0;
            }
        }

        // make simhzx by solving Riemann problem
        simhzx(i,j,k) = (wmac(i,j,k) > 0.0) ?
            slzx : srzx;
        simhzx(i,j,k) = (fabs(wmac(i,j,k)) > rel_eps) ?
            simhzx(i,j,k) : 0.5 * (slzx + srzx);
    });

    // simhzy
    // imhbox = mfi.grownnodaltilebox(2, IntVect(1,0,0));
    imhbox = amrex::grow(mfi.tilebox(), 0, 1);
    imhbox = amrex::growHi(imhbox, 2, 1);

    AMREX_PARALLEL_FOR_3D(imhbox, i, j, k, 
    {
        Real slzy = 0.0;
        Real srzy = 0.0;
        // loop over appropriate zy faces
        if (is_conservative) {
            // make slzy, srzy by updating 1D extrapolation
            slzy = slz(i,j,k) 
                - (dt3/hy) * (simhy(i,j+1,k-1)*vmac(i,j+1,k-1) 
                - simhy(i,j,k-1)*vmac(i,j,k-1)) 
                - dt3*scal(i,j,k-1,comp)*divu(i,j,k-1) 
                + (dt3/hy)*scal(i,j,k-1,comp)*
                (vmac(i,j+1,k-1)-vmac(i,j,k-1));
            srzy = srz(i,j,k) 
                - (dt3/hy)*(simhy(i,j+1,k)*vmac(i,j+1,k)
                - simhy(i,j,k)*vmac(i,j,k)) 
                - dt3*scal(i,j,k,comp)*divu(i,j,k) 
                + (dt3/hy)*scal(i,j,k,comp)*
                (vmac(i,j+1,k)-vmac(i,j,k));
        } else {
            // make slzy, srzy by updating 1D extrapolation
            slzy = slz(i,j,k) 
                - (dt6/hy)*(vmac(i,j+1,k-1)+vmac(i,j,k-1)) 
                *(simhy(i,j+1,k-1)-simhy(i,j,k-1));
            srzy = srz(i,j,k) 
                - (dt6/hy)*(vmac(i,j+1,k)+vmac(i,j,k)) 
                *(simhy(i,j+1,k)-simhy(i,j,k));
        }

        // impose lo side bc's
        if (k == klo) {
            if (bclo == EXT_DIR) {
                slzy = scal(i,j,k-1,comp);
                srzy = scal(i,j,k-1,comp);
            } else if (bclo ==  FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 2) {
                    srzy = min(srzy,0.0);
                }
                slzy = srzy;
            } else if (bclo == REFLECT_EVEN) {
                slzy = srzy;
            } else if (bclo ==  REFLECT_ODD) {
                slzy = 0.0;
                srzy = 0.0;
            }

        // impose hi side bc's
        } else if (k == khi+1) {
            if (bchi == EXT_DIR) {
                slzy = scal(i,j,k,comp);
                srzy = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 2) {
                    slzy = max(slzy,0.0);
                }
                srzy = slzy;
            } else if (bchi == REFLECT_EVEN) {
                srzy = slzy;
            } else if (bchi == REFLECT_ODD) {
                slzy = 0.0;
                srzy = 0.0;
            }
        }

        // make simhzy by solving Riemann problem
        simhzy(i,j,k) = (wmac(i,j,k) > 0.0) ?
            slzy : srzy;
        simhzy(i,j,k) = (fabs(wmac(i,j,k)) > rel_eps) ?
            simhzy(i,j,k) : 0.5 * (slzy + srzy);
    });
}

void Maestro::MakeEdgeScalEdges(const MFIter& mfi,
                            Array4<Real> const slx,
                            Array4<Real> const srx,
                            Array4<Real> const sly,
                            Array4<Real> const sry,
                            Array4<Real> const slz,
                            Array4<Real> const srz,
                            Array4<Real> const scal,
                            Array4<Real> const sedgex,
                            Array4<Real> const sedgey,
                            Array4<Real> const sedgez,
                            Array4<Real> const force,
                            Array4<Real> const umac,
                            Array4<Real> const vmac,
                            Array4<Real> const wmac,
                            Array4<Real> const Ipf,
                            Array4<Real> const Imf,
                            Array4<Real> const simhxy,
                            Array4<Real> const simhxz,
                            Array4<Real> const simhyx,
                            Array4<Real> const simhyz,
                            Array4<Real> const simhzx,
                            Array4<Real> const simhzy,
                            const Box& domainBox,
                            const Vector<BCRec>& bcs,
                            const Real* dx,
                            int comp, int bccomp, 
                            bool is_vel, bool is_conservative) 
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeEdgeScalEdges()",MakeEdgeScalEdges);

    ///////////////////////////////////////////////
    // Create sedgelx, etc.
    ///////////////////////////////////////////////

    int ppm_trace_forces_local = ppm_trace_forces;

    Real dt2 = 0.5 * dt;
    Real dt4 = 0.25 * dt;

    Real hx = dx[0];
    Real hy = dx[1];
    Real hz = dx[2];

    int ilo = domainBox.loVect()[0];
    int ihi = domainBox.hiVect()[0];
    int jlo = domainBox.loVect()[1];
    int jhi = domainBox.hiVect()[1];
    int klo = domainBox.loVect()[2];
    int khi = domainBox.hiVect()[2];

    const Box& xbx = mfi.nodaltilebox(0);
    const Box& ybx = mfi.nodaltilebox(1);
    const Box& zbx = mfi.nodaltilebox(2);

    const Real rel_eps = c_rel_eps;

    // x-direction
    int bclo = bcs[bccomp].lo()[0];
    int bchi = bcs[bccomp].hi()[0];
    AMREX_PARALLEL_FOR_3D(xbx, i, j, k, 
    {
        Real sedgelx = 0.0;
        Real sedgerx = 0.0;

        Real fl = (ppm_trace_forces_local == 0) ? 
            force(i-1,j,k,comp) : Ipf(i-1,j,k,0);
        Real fr = (ppm_trace_forces_local == 0) ? 
            force(i,j,k,comp) : Imf(i,j,k,0);

        // make sedgelx, sedgerx
        if (is_conservative) {
            sedgelx = slx(i,j,k) 
                - (dt2/hy)*(simhyz(i-1,j+1,k  )*vmac(i-1,j+1,k) 
                - simhyz(i-1,j,k)*vmac(i-1,j,k)) 
                - (dt2/hz)*(simhzy(i-1,j,k+1)*wmac(i-1,j,k+1) 
                - simhzy(i-1,j,k)*wmac(i-1,j,k)) 
                - (dt2/hx)*scal(i-1,j,k,comp)*(umac(i,j,k)-umac(i-1,j,k)) 
                + dt2*fl;

            sedgerx = srx(i,j,k) 
                - (dt2/hy)*(simhyz(i,j+1,k)*vmac(i,j+1,k) 
                - simhyz(i,j,k)*vmac(i,j,k)) 
                - (dt2/hz)*(simhzy(i,j,k+1)*wmac(i,j,k+1) 
                - simhzy(i,j,k)*wmac(i,j,k)) 
                - (dt2/hx)*scal(i,j,k,comp)*(umac(i+1,j,k)-umac(i,j,k)) 
                + dt2*fr;
        } else {
            sedgelx = slx(i,j,k) 
                - (dt4/hy)*(vmac(i-1,j+1,k)+vmac(i-1,j,k))* 
                (simhyz(i-1,j+1,k)-simhyz(i-1,j,k)) 
                - (dt4/hz)*(wmac(i-1,j,k+1)+wmac(i-1,j,k))* 
                (simhzy(i-1,j,k+1)-simhzy(i-1,j,k)) 
                + dt2*fl;

            sedgerx = srx(i,j,k) 
                - (dt4/hy)*(vmac(i,j+1,k)+vmac(i,j,k))* 
                (simhyz(i,j+1,k)-simhyz(i,j,k)) 
                - (dt4/hz)*(wmac(i,j,k+1)+wmac(i,j,k))* 
                (simhzy(i,j,k+1)-simhzy(i,j,k)) 
                + dt2*fr;
        } 

        // make sedgex by solving Riemann problem
        // boundary conditions enforced outside of i,j,k loop
        sedgex(i,j,k,comp) = (umac(i,j,k) > 0.0) ? 
            sedgelx : sedgerx;
        sedgex(i,j,k,comp) = (fabs(umac(i,j,k))  > rel_eps) ? 
            sedgex(i,j,k,comp) : 0.5*(sedgelx+sedgerx);

        // impose lo side bc's
        if (i == ilo) {
            if (bclo == EXT_DIR) {
                sedgex(i,j,k,comp) = scal(i-1,j,k,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    sedgex(i,j,k,comp) = min(sedgerx,0.0);
                } else {
                    sedgex(i,j,k,comp) = sedgerx;
                } 
            } else if (bclo == REFLECT_EVEN) {
                sedgex(i,j,k,comp) = sedgerx;
            } else if (bclo == REFLECT_ODD) {
                sedgex(i,j,k,comp) = 0.0;
            }

        // impose hi side bc's
        } else if (i == ihi+1) {
            if (bchi == EXT_DIR) {
                sedgex(i,j,k,comp) = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 0) {
                    sedgex(i,j,k,comp) = max(sedgelx,0.0);
                } else {
                    sedgex(i,j,k,comp) = sedgelx;
                } 
            } else if (bchi == REFLECT_EVEN) {
                sedgex(i,j,k,comp) = sedgelx;
            } else if (bchi == REFLECT_ODD) {
                sedgex(i,j,k,comp) = 0.0;
            } 
        } 
    });

    // y-direction
    bclo = bcs[bccomp].lo()[1];
    bchi = bcs[bccomp].hi()[1];
    AMREX_PARALLEL_FOR_3D(ybx, i, j, k, 
    {
        Real sedgely = 0.0;
        Real sedgery = 0.0;

        Real fl = (ppm_trace_forces_local == 0) ? 
            force(i,j-1,k,comp) : Ipf(i,j-1,k,1);
        Real fr = (ppm_trace_forces_local == 0) ? 
            force(i,j,k,comp) : Imf(i,j,k,1);

        // make sedgely, sedgery
        if (is_conservative) {
            sedgely = sly(i,j,k) 
                - (dt2/hx)*(simhxz(i+1,j-1,k  )*umac(i+1,j-1,k) 
                - simhxz(i,j-1,k)*umac(i,j-1,k)) 
                - (dt2/hz)*(simhzx(i,j-1,k+1)*wmac(i,j-1,k+1) 
                - simhzx(i,j-1,k)*wmac(i,j-1,k)) 
                - (dt2/hy)*scal(i,j-1,k,comp)*(vmac(i,j,k)-vmac(i,j-1,k)) 
                + dt2*fl;

            sedgery = sry(i,j,k) 
                - (dt2/hx)*(simhxz(i+1,j,k)*umac(i+1,j,k) 
                - simhxz(i,j,k)*umac(i,j,k)) 
                - (dt2/hz)*(simhzx(i,j,k+1)*wmac(i,j,k+1) 
                - simhzx(i,j,k)*wmac(i,j,k)) 
                - (dt2/hy)*scal(i,j,k,comp)*(vmac(i,j+1,k)-vmac(i,j,k)) 
                + dt2*fr;
        } else {
            sedgely = sly(i,j,k) 
                - (dt4/hx)*(umac(i+1,j-1,k)+umac(i,j-1,k))* 
                (simhxz(i+1,j-1,k)-simhxz(i,j-1,k)) 
                - (dt4/hz)*(wmac(i,j-1,k+1)+wmac(i,j-1,k))* 
                (simhzx(i,j-1,k+1)-simhzx(i,j-1,k)) 
                + dt2*fl;

            sedgery = sry(i,j,k) 
                - (dt4/hx)*(umac(i+1,j,k)+umac(i,j,k))* 
                (simhxz(i+1,j,k)-simhxz(i,j,k)) 
                - (dt4/hz)*(wmac(i,j,k+1)+wmac(i,j,k))* 
                (simhzx(i,j,k+1)-simhzx(i,j,k)) 
                + dt2*fr;
        } 

        // make sedgey by solving Riemann problem
        // boundary conditions enforced outside of i,j,k loop
        sedgey(i,j,k,comp) = (vmac(i,j,k) > 0.0) ? 
            sedgely : sedgery;
        sedgey(i,j,k,comp) = (fabs(vmac(i,j,k))  > rel_eps) ? 
            sedgey(i,j,k,comp) : 0.5*(sedgely+sedgery);

        // impose lo side bc's
        if (j == jlo) {
            if (bclo == EXT_DIR) {
                sedgey(i,j,k,comp) = scal(i,j-1,k,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sedgey(i,j,k,comp) = min(sedgery,0.0);
                } else {
                    sedgey(i,j,k,comp) = sedgery;
                } 
            } else if (bclo == REFLECT_EVEN) {
                sedgey(i,j,k,comp) = sedgery;
            } else if (bclo == REFLECT_ODD) {
                sedgey(i,j,k,comp) = 0.0;
            }

        // impose hi side bc's
        } else if (j == jhi+1) {
            if (bchi == EXT_DIR) {
                sedgey(i,j,k,comp) = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 1) {
                    sedgey(i,j,k,comp) = max(sedgely,0.0);
                } else {
                    sedgey(i,j,k,comp) = sedgely;
                } 
            } else if (bchi == REFLECT_EVEN) {
                sedgey(i,j,k,comp) = sedgely;
            } else if (bchi == REFLECT_ODD) {
                sedgey(i,j,k,comp) = 0.0;
            } 
        } 
    });

    // z-direction
    bclo = bcs[bccomp].lo()[2];
    bchi = bcs[bccomp].hi()[2];
    AMREX_PARALLEL_FOR_3D(zbx, i, j, k, 
    {
        Real sedgelz = 0.0;
        Real sedgerz = 0.0;

        Real fl = (ppm_trace_forces_local == 0) ? 
            force(i,j,k-1,comp) : Ipf(i,j,k-1,2);
        Real fr = (ppm_trace_forces_local == 0) ? 
            force(i,j,k,comp) : Imf(i,j,k,2);

        // make sedgelz, sedgerz
        if (is_conservative) {
            sedgelz = slz(i,j,k) 
                - (dt2/hx)*(simhxy(i+1,j,k-1)*umac(i+1,j,k-1) 
                - simhxy(i,j,k-1)*umac(i,j,k-1)) 
                - (dt2/hy)*(simhyx(i,j+1,k-1)*vmac(i,j+1,k-1) 
                - simhyx(i,j,k-1)*vmac(i,j,k-1)) 
                - (dt2/hz)*scal(i,j,k-1,comp)*(wmac(i,j,k)-wmac(i,j,k-1)) 
                + dt2*fl;

            sedgerz = srz(i,j,k) 
                - (dt2/hx)*(simhxy(i+1,j,k)*umac(i+1,j,k) 
                - simhxy(i,j,k)*umac(i,j,k)) 
                - (dt2/hy)*(simhyx(i,j+1,k)*vmac(i,j+1,k) 
                - simhyx(i,j,k)*vmac(i,j,k)) 
                - (dt2/hz)*scal(i,j,k,comp)*(wmac(i,j,k+1)-wmac(i,j,k)) 
                + dt2*fr;
        } else {
            sedgelz = slz(i,j,k) 
                - (dt4/hx)*(umac(i+1,j,k-1)+umac(i,j,k-1))* 
                (simhxy(i+1,j,k-1)-simhxy(i,j,k-1)) 
                - (dt4/hy)*(vmac(i,j+1,k-1)+vmac(i,j,k-1))* 
                (simhyx(i,j+1,k-1)-simhyx(i,j,k-1)) 
                + dt2*fl;

            sedgerz = srz(i,j,k) 
                - (dt4/hx)*(umac(i+1,j,k)+umac(i,j,k))* 
                (simhxy(i+1,j,k)-simhxy(i,j,k)) 
                - (dt4/hy)*(vmac(i,j+1,k)+vmac(i,j,k))* 
                (simhyx(i,j+1,k)-simhyx(i,j,k)) 
                + dt2*fr;
        } 

        // make sedgez by solving Riemann problem
        // boundary conditions enforced outside of i,j,k loop
        sedgez(i,j,k,comp) = (wmac(i,j,k) > 0.0) ? 
            sedgelz : sedgerz;
        sedgez(i,j,k,comp) = (fabs(wmac(i,j,k))  > rel_eps) ? 
            sedgez(i,j,k,comp) : 0.5*(sedgelz+sedgerz);

        // impose lo side bc's
        if (k == klo) {
            if (bclo == EXT_DIR) {
                sedgez(i,j,k,comp) = scal(i,j,k-1,comp);
            } else if (bclo == FOEXTRAP || bclo == HOEXTRAP) {
                if (is_vel && comp == 2) {
                    sedgez(i,j,k,comp) = min(sedgerz,0.0);
                } else {
                    sedgez(i,j,k,comp) = sedgerz;
                } 
            } else if (bclo == REFLECT_EVEN) {
                sedgez(i,j,k,comp) = sedgerz;
            } else if (bclo == REFLECT_ODD) {
                sedgez(i,j,k,comp) = 0.0;
            }

        // impose hi side bc's
        } else if (k == khi+1) {
            if (bchi == EXT_DIR) {
                sedgez(i,j,k,comp) = scal(i,j,k,comp);
            } else if (bchi == FOEXTRAP || bchi == HOEXTRAP) {
                if (is_vel && comp == 2) {
                    sedgez(i,j,k,comp) = max(sedgelz,0.0);
                } else {
                    sedgez(i,j,k,comp) = sedgelz;
                } 
            } else if (bchi == REFLECT_EVEN) {
                sedgez(i,j,k,comp) = sedgelz;
            } else if (bchi == REFLECT_ODD) {
                sedgez(i,j,k,comp) = 0.0;
            } 
        } 
    });
}

#endif