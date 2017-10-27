
#include <Maestro.H>

using namespace amrex;

// advance solution to final time
void
Maestro::Evolve ()
{
    int last_plot_file_step = 0;

    for (istep = 1; istep <= max_step && t_new < stop_time; ++istep)
    {

        // check to see if we need to regrid, then regrid
        Regrid(istep);

        // move snew into sold by swapping pointers
        for (int lev=0; lev<=finest_level; ++lev) {
            std::swap(sold[lev], snew[lev]);
        }

        // compute time step
        ComputeDt();

        // reset t_old and t_new
        t_old = t_new;
        t_new += dt;

        // advance the solution by dt
        AdvanceTimeStep(false);

        // write a plotfile
        if (plot_int > 0 && istep % plot_int == 0)
        {
            Print() << "\nWriting plotfile " << istep << std::endl;
            last_plot_file_step = istep;
            WritePlotFile(istep);
        }

    }

    // write a final plotfile if we haven't already
    if (plot_int > 0 && istep > last_plot_file_step)
    {
        Print() << "\nWriting plotfile " << istep-1 << std::endl;
        WritePlotFile(istep-1);
    }
}