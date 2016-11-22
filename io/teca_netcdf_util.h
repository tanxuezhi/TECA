#ifndef teca_netcdf_util_h
#define teca_netcdf_util_h

#include <netcdf.h>

// macro to help with netcdf data types
#define NC_DISPATCH_FP(tc_, code_)                          \
    switch (tc_)                                            \
    {                                                       \
    NC_DISPATCH_CASE(NC_FLOAT, float, code_)                \
    NC_DISPATCH_CASE(NC_DOUBLE, double, code_)              \
    default:                                                \
        TECA_ERROR("netcdf type code_ " << tc_              \
            << " is not a floating point type")             \
    }

#define NC_DISPATCH(tc_, code_)                             \
    switch (tc_)                                            \
    {                                                       \
    NC_DISPATCH_CASE(NC_BYTE, char, code_)                  \
    NC_DISPATCH_CASE(NC_UBYTE, unsigned char, code_)        \
    NC_DISPATCH_CASE(NC_CHAR, char, code_)                  \
    NC_DISPATCH_CASE(NC_SHORT, short int, code_)            \
    NC_DISPATCH_CASE(NC_USHORT, unsigned short int, code_)  \
    NC_DISPATCH_CASE(NC_INT, int, code_)                    \
    NC_DISPATCH_CASE(NC_UINT, unsigned int, code_)          \
    NC_DISPATCH_CASE(NC_INT64, long long, code_)            \
    NC_DISPATCH_CASE(NC_UINT64, unsigned long long, code_)  \
    NC_DISPATCH_CASE(NC_FLOAT, float, code_)                \
    NC_DISPATCH_CASE(NC_DOUBLE, double, code_)              \
    default:                                                \
        TECA_ERROR("netcdf type code_ " << tc_              \
            << " is not supported")                         \
    }

#define NC_DISPATCH_CASE(cc_, tt_, code_)   \
    case cc_:                               \
    {                                       \
        using NC_T = tt_;                   \
        code_                               \
        break;                              \
    }

namespace teca_netcdf_util
{

// traits class mapping to netcdf
template<typename num_t> class netcdf_tt {};

#define DECLARE_NETCDF_TT(cpp_t_, nc_c_) \
template <> class netcdf_tt<cpp_t_>      \
{ public: enum { type_code = nc_c_ }; };
DECLARE_NETCDF_TT(char, NC_BYTE)
DECLARE_NETCDF_TT(unsigned char, NC_UBYTE)
DECLARE_NETCDF_TT(char, NC_CHAR)
DECLARE_NETCDF_TT(short int, NC_SHORT)
DECLARE_NETCDF_TT(unsigned short int, NC_USHORT)
DECLARE_NETCDF_TT(int, NC_INT)
DECLARE_NETCDF_TT(unsigned int, NC_UINT)
DECLARE_NETCDF_TT(long long, NC_INT64)
DECLARE_NETCDF_TT(unsigned long long, NC_UINT64)
DECLARE_NETCDF_TT(float, NC_FLOAT)
DECLARE_NETCDF_TT(double, NC_DOUBLE)

// to deal with fortran fixed length strings
// which are not properly null terminated
static void crtrim(char *s, long n)
{
    if (!s || (n == 0)) return;
    char c = s[--n];
    while ((n > 0) && ((c == ' ') || (c == '\n') ||
        (c == '\t') || (c == '\r')))
    {
        s[n] = '\0';
        c = s[--n];
    }
}

// RAII for managing netcdf files
class netcdf_handle
{
public:
    // initialize with a handle returned from
    // nc_open/nc_create etc
    netcdf_handle(int h) : m_handle(h)
    {}

    // close the file during destruction
    ~netcdf_handle()
    { this->close(); }

    // this is a move only class, and should
    // only be initialized with an valid handle
    netcdf_handle() = delete;
    netcdf_handle(const netcdf_handle &) = delete;
    void operator=(const netcdf_handle &) = delete;

    // move construction takes ownership
    // from the other object
    netcdf_handle(netcdf_handle &&other)
    {
        m_handle = other.m_handle;
        other.m_handle = 0;
    }

    // move assignment takes ownership
    // from the other object
    void operator=(netcdf_handle &&other)
    {
        this->close();
        m_handle = other.m_handle;
        other.m_handle = 0;
    }

    // close the file
    void close()
    {
        if (m_handle)
        {
            nc_close(m_handle);
            m_handle = 0;
        }
    }

    // dereference a pointer to the object
    // returns a reference to the handle
    int &get()
    { return m_handle; }

private:
    int m_handle;
};
};

#endif
