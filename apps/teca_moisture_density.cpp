#include "teca_config.h"
#include "teca_apply_binary_mask.h"
#include "teca_binary_segmentation.h"
#include "teca_cf_reader.h"
#include "teca_vtk_cartesian_mesh_writer.h"
#include "teca_programmable_reduce.h"
#include "teca_array_collection.h"
#include "teca_variant_array.h"
#include "teca_metadata.h"
#include "teca_mpi_manager.h"
#include "teca_coordinate_util.h"
#include "calcalcs.h"

#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <boost/program_options.hpp>

using namespace std;

using boost::program_options::value;

using seconds_t =
    std::chrono::duration<double, std::chrono::seconds::period>;

p_teca_dataset mesh_accumulate(const const_p_teca_dataset &leftds,
        const const_p_teca_dataset &rightds)
{
    if (rightds && !leftds)
    {
        return std::const_pointer_cast<teca_dataset>(rightds);
    }
    else if (!rightds && leftds)
    {
        return std::const_pointer_cast<teca_dataset>(leftds);
    }
    else if (leftds && rightds)
    {
        const_p_teca_mesh left =
            std::dynamic_pointer_cast<const teca_mesh>(leftds);
        if (!left)
        {
            TECA_ERROR("left is not a teca_mesh");
            return nullptr;
        }

        const_p_teca_mesh right =
            std::dynamic_pointer_cast<const teca_mesh>(rightds);

        if (!right)
        {
            TECA_ERROR("right is not a teca_mesh");
            return nullptr;
        }

        p_teca_mesh out =
            std::static_pointer_cast<teca_mesh>(left->new_instance());

        out->copy_metadata(left);

        /*unsigned long lstep = 0;
        left->get_metadata().get("time_step", lstep);
        unsigned long rstep = 0;
        right->get_metadata().get("time_step", rstep);
        cerr << "processing steps " << lstep << " and " << rstep << endl;*/

        const_p_teca_array_collection larrays = left->get_point_arrays();
        const_p_teca_array_collection rarrays = right->get_point_arrays();

        unsigned int narrays = larrays->size();
        for (unsigned int q = 0; q < narrays; ++q)
        {
            const_p_teca_variant_array larray = larrays->get(q);
            const_p_teca_variant_array rarray = rarrays->get(q);

            unsigned long nelem = larray->size();

            p_teca_variant_array oarray = larray->new_instance(nelem);

            TEMPLATE_DISPATCH(teca_variant_array_impl,
                oarray.get(),

                const NT *pl = static_cast<const TT*>(larray.get())->get();
                const NT *pr = static_cast<const TT*>(rarray.get())->get();
                NT *po = static_cast<TT*>(oarray.get())->get();

                for (unsigned long i = 0; i < nelem; ++i)
                {
                    po[i] = pl[i] + pr[i];
                }
                )

            out->get_point_arrays()->set(larrays->get_name(q), oarray);
        }
        return out;
    }

    return nullptr;
}


// --------------------------------------------------------------------------
int main(int argc, char **argv)
{
    // initialize mpi
    teca_mpi_manager mpi_man(argc, argv);

    std::chrono::high_resolution_clock::time_point t0, t1;
    if (mpi_man.get_comm_rank() == 0)
        t0 = std::chrono::high_resolution_clock::now();

    // initialize command line options description
    // set up some common options to simplify use for most
    // common scenarios
    options_description basic_opt_defs(
        "Basic usage:\n\n"
        "The following options are the most commonly used. Information\n"
        "on advanced options can be displayed using --advanced_help\n\n"
        "Basic command line options", 120, -1
        );
    basic_opt_defs.add_options()
        ("input_file", value<string>(), "file path to the simulation data")
        ("input_regex", value<string>(), "regex matching simulation data files")
        ("moisture_variable", value<string>(), "name of variable with integrated moisture (TMQ)")
        ("low_moisture_threshold", value<double>(), "low cut off used in segmentation (-inf)")
        ("high_moisture_threshold", value<double>(), "high cut off used in segmentation (+inf)")
        ("output_file", value<string>(), "file to write moisture density (moisture_density.%e%)")
        ("first_step", value<long>(), "first time step to process")
        ("last_step", value<long>(), "last time step to process")
        ("start_date", value<string>(), "first time to proces in YYYY-MM-DD hh:mm:ss format")
        ("end_date", value<string>(), "first time to proces in YYYY-MM-DD hh:mm:ss format")
        ("n_threads", value<int>(), "thread pool size. default is 1. -1 for all")
        ("help", "display the basic options help")
        ("advanced_help", "display the advanced options help")
        ("full_help", "display entire help message")
        ;

    // add all options from each pipeline stage for more advanced use
    options_description advanced_opt_defs(
        "Advanced usage:\n\n"
        "The following list contains the full set options giving one full\n"
        "control over all runtime modifiable parameters. The basic options\n"
        "(see" "--help) map to these, and will override them if both are\n"
        "specified.\n\n"
        "moisture density pipeline:\n\n"
        "  (reader)\n"
        "     \\\n"
        "   (segment)--(mask)--(accum)\n"
        "                         \\\n"
        "                       (writer)\n\n"
        "Advanced command line options", -1, 1
        );

    // create the pipeline stages here, they contain the
    // documentation and parse command line.
    // objects report all of their properties directly
    // set default options here so that command line options override
    // them. while we are at it connect the pipeline
    p_teca_cf_reader reader = teca_cf_reader::New();
    reader->get_properties_description("reader", advanced_opt_defs);

    p_teca_binary_segmentation segment = teca_binary_segmentation::New();
    segment->set_input_connection(reader->get_output_port());
    segment->set_threshold_variable("prw");
    segment->set_segmentation_variable("moisture_mask");
    segment->get_properties_description("segment", advanced_opt_defs);

    p_teca_apply_binary_mask mask = teca_apply_binary_mask::New();
    mask->set_input_connection(segment->get_output_port());
    mask->set_mask_variable("moisture_mask");
    mask->get_properties_description("mask", advanced_opt_defs);

    p_teca_programmable_reduce accum = teca_programmable_reduce::New();
    accum->set_input_connection(mask->get_output_port());
    accum->set_reduce_callback(mesh_accumulate);
    accum->get_properties_description("accum", advanced_opt_defs);

    p_teca_vtk_cartesian_mesh_writer writer = teca_vtk_cartesian_mesh_writer::New();
    writer->set_input_connection(accum->get_output_port());
    writer->set_file_name("moisture_density.%e%");
    writer->get_properties_description("writer", advanced_opt_defs);

    // package basic and advanced options for display
    options_description all_opt_defs(-1, -1);
    all_opt_defs.add(basic_opt_defs).add(advanced_opt_defs);

    // parse the command line
    variables_map opt_vals;
    try
    {
        boost::program_options::store(
            boost::program_options::command_line_parser(argc, argv).options(all_opt_defs).run(),
            opt_vals);

        if (mpi_man.get_comm_rank() == 0)
        {
            if (opt_vals.count("help"))
            {
                cerr << endl
                    << "usage: teca_moisture_densisty [options]" << endl
                    << endl
                    << basic_opt_defs << endl
                    << endl;
                return -1;
            }
            if (opt_vals.count("advanced_help"))
            {
                cerr << endl
                    << "usage: teca_moisture_density [options]" << endl
                    << endl
                    << advanced_opt_defs << endl
                    << endl;
                return -1;
            }

            if (opt_vals.count("full_help"))
            {
                cerr << endl
                    << "usage: teca_moisture_density [options]" << endl
                    << endl
                    << all_opt_defs << endl
                    << endl;
                return -1;
            }
        }

        boost::program_options::notify(opt_vals);
    }
    catch (std::exception &e)
    {
        TECA_ERROR("Error parsing command line options. See --help "
            "for a list of supported options. " << e.what())
        return -1;
    }

    // pass command line arguments into the pipeline objects
    // advanced options are processed first, so that the basic
    // options will override them
    reader->set_properties("reader", opt_vals);
    segment->set_properties("segment", opt_vals);
    mask->set_properties("mask", opt_vals);
    accum->set_properties("accum", opt_vals);
    writer->set_properties("writer", opt_vals);

    // now pass in the basic options, these are processed
    // last so that they will take precedence
    if (opt_vals.count("input_file"))
        reader->set_file_name(
            opt_vals["input_file"].as<string>());

    if (opt_vals.count("input_regex"))
        reader->set_files_regex(
            opt_vals["input_regex"].as<string>());

    if (opt_vals.count("moisture_variable"))
        segment->set_threshold_variable(
            opt_vals["moisture_variable"].as<string>());

    if (opt_vals.count("low_moisture_threshold"))
        segment->set_low_threshold_value(
            opt_vals["low_moisture_threshold"].as<double>());

    if (opt_vals.count("high_moisture_threshold"))
        segment->set_high_threshold_value(
            opt_vals["high_moisture_threshold"].as<double>());

    if (opt_vals.count("first_step"))
        accum->set_first_step(opt_vals["first_step"].as<long>());

    if (opt_vals.count("last_step"))
        accum->set_last_step(opt_vals["last_step"].as<long>());

    if (opt_vals.count("n_threads"))
        accum->set_thread_pool_size(opt_vals["n_threads"].as<int>());

    if (opt_vals.count("output_file"))
        writer->set_file_name(opt_vals["output_file"].as<string>());


    // some minimal check for missing options
    if (reader->get_file_name().empty()
        && reader->get_files_regex().empty())
    {
        if (mpi_man.get_comm_rank() == 0)
        {
            TECA_ERROR(
                "missing file name or regex for simulation reader. "
                "See --help for a list of command line options.")
        }
        return -1;
    }

    // look for requested time step range, start
    bool parse_start_date = opt_vals.count("start_date");
    bool parse_end_date = opt_vals.count("end_date");
    if (parse_start_date || parse_end_date)
    {
        // run the reporting phase of the pipeline
        teca_metadata md = reader->update_metadata();

        teca_metadata atrs;
        if (md.get("attributes", atrs))
        {
            TECA_ERROR("metadata mising attributes")
            return -1;
        }

        teca_metadata time_atts;
        std::string calendar;
        std::string units;
        if (atrs.get("time", time_atts)
           || time_atts.get("calendar", calendar)
           || time_atts.get("units", units))
        {
            TECA_ERROR("failed to determine the calendaring parameters")
            return -1;
        }

        teca_metadata coords;
        p_teca_double_array time;
        if (md.get("coordinates", coords) ||
            !(time = std::dynamic_pointer_cast<teca_double_array>(
                coords.get("t"))))
        {
            TECA_ERROR("failed to determine time coordinate")
            return -1;
        }

        // convert date string to step, start date
        if (parse_start_date)
        {
            unsigned long first_step = 0;
            std::string start_date = opt_vals["start_date"].as<string>();
            if (teca_coordinate_util::time_step_of(time, true, calendar,
                 units, start_date, first_step))
            {
                TECA_ERROR("Failed to lcoate time step for start date \""
                    <<  start_date << "\"")
                return -1;
            }
            accum->set_first_step(first_step);
        }

        // and end date
        if (parse_end_date)
        {
            unsigned long last_step = 0;
            std::string end_date = opt_vals["end_date"].as<string>();
            if (teca_coordinate_util::time_step_of(time, false, calendar,
                 units, end_date, last_step))
            {
                TECA_ERROR("Failed to lcoate time step for end date \""
                    <<  end_date << "\"")
                return -1;
            }
            accum->set_last_step(last_step);
        }
    }

    // run the pipeline
    writer->update();

    if (mpi_man.get_comm_rank() == 0)
    {
        t1 = std::chrono::high_resolution_clock::now();
        seconds_t dt(t1 - t0);
        TECA_STATUS("teca_moisture_density run_time=" << dt.count() << " sec")
    }

    return 0;
}
