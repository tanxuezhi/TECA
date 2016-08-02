#ifndef teca_cf_writer_h
#define teca_cf_writer_h

#include "teca_algorithm.h"
#include "teca_metadata.h"
#include "teca_shared_object.h"

#include <vector>
#include <string>
#include <mutex>

TECA_SHARED_OBJECT_FORWARD_DECL(teca_cf_writer)

class teca_cf_writer_internals;
using p_teca_cf_writer_internals = std::shared_ptr<teca_cf_writer_internals>;

/// a writer for data stored in NetCDF CF format
/**
a writer for data stored in NetCDF CF format

writes a set of arrays from single time step into a cartesian
mesh.

metadata keys:
    variables - a list of all available variables.
    <var> -  a metadata object holding all NetCDF attributes for the variable named <var>
    time variables - a list of all variables with time as the only dimension
    coordinates - a metadata object holding names and arrays of the coordinate axes
        x_variable - name of x axis variable
        y_variable - name of y axis variable
        z_variable - name of z axis variable
        t_variable - name of t axis variable
        x - array of x coordinates
        y - array of y coordinates
        z - array of z coordinates
        t - array of t coordinates
    files - list of files in this dataset
    step_count - list of the number of steps in each file
    number_of_time_steps - total number of time steps in all files
    whole_extent - index space extent describing (nodal) dimensions of the mesh

request keys:
    time_step - the time step to write
    arrays - list of arrays to write
    extent - index space extents describing the subset of data to write

output:
    generates a 1,2 or 3D cartesian mesh for the requested timestep
    on the requested extent with the requested point based arrays
    and value at this timestep for all time variables.
*/
class teca_cf_writer : public teca_algorithm
{
public:
    TECA_ALGORITHM_STATIC_NEW(teca_cf_writer)
    ~teca_cf_writer();

    TECA_ALGORITHM_DELETE_COPY_ASSIGN(teca_cf_writer)

    // report/initialize to/from Boost program options
    // objects.
    TECA_GET_ALGORITHM_PROPERTIES_DESCRIPTION()
    TECA_SET_ALGORITHM_PROPERTIES()

    // a file name to write to.
    TECA_ALGORITHM_PROPERTY(std::string, file_name)

    // set the file mode, append or truncate
    TECA_ALGORITHM_PROPERTY(int, file_mode)

    // set the number of time steps to store per file.
    // default 16. use -1 to indicate time should be an
    // unlimted dimension. however, note that the file
    // access must be sequential in that case.
    TECA_ALGORITHM_PROPERTY(unsigned long, steps_per_file)

protected:
    teca_cf_writer();

private:
    const_p_teca_dataset execute(
        unsigned int port,
        const std::vector<const_p_teca_dataset> &input_data,
        const teca_metadata &request) override;

private:
    std::string file_name;
    int file_mode;

    p_teca_cf_writer_internals internals;
};

#endif
