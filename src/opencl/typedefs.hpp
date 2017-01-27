#ifndef VOPENCL_TYPEDEFS_HPP_
#define VOPENCL_TYPEDEFS_HPP_

#include <cassert>
#include <type_traits>
#include <vector>

#include "internal.hpp"
#include "opencl_include.hpp"
#include "opencl_utils.hpp"

namespace vcl = ::vopencl::internal;

namespace vopencl
{
   namespace internal
   {
#ifdef OPENCL_DP
      typedef cl_double real_t;
#else
      typedef cl_float  real_t;
#endif // OPENCL_DP


      // Class for device buffer where each element is a 3 component vector.
      // i.e. stored as x0,y0,z0,x1,y1,z1,...
      // As the hardware will read from memory in chunks (typically 128bits)
      // this will allow a work item to read the x,y,z components in fewer
      // reads than if they were in different locations.

      template <typename T>
      class Buffer3D
      {
         std::vector<cl::Buffer> buff_container;

         // number of elements in one dimension
         size_t n_elems;

         size_t buffer_size;

#ifdef USE_VECTOR_TYPE
#ifdef OPENCL_DP
         typedef cl_double3 Rv;
#else
         typedef cl_float3 Rv;
#endif // OPENCL_DP
         unsigned v = 1;
#else
         typedef T Rv;
         unsigned v = 3;
#endif // USE_VECTOR_TYPE

      public:

         Buffer3D(void) noexcept : buff_container(0), n_elems(0), buffer_size(0)  {}

         // initialize without writing, but with size
         // e.g. for use when generating buffer on device
         Buffer3D(const cl::Context &c,
                  const cl_mem_flags fs,
                  const size_t n) noexcept
            : buff_container(1), n_elems(n), buffer_size(n*v*sizeof(Rv))
         {
            buff_container[0] = cl::Buffer(c, fs, v*n_elems * sizeof(Rv));
         }

         // initialize with 3 vectors to write data to device
         // use template so that a Buffer3D<float> can be initialised
         // with std::vector<double>s
         template <typename R>
         Buffer3D(const cl::Context &c,
                  const cl::CommandQueue &q,
                  const cl_mem_flags fs,
                  const std::vector<R> &xs,
                  const std::vector<R> &ys,
                  const std::vector<R> &zs) noexcept
            : buff_container(1)
         {
            n_elems = xs.size();
            buffer_size = n_elems * v * sizeof(Rv);

            buff_container[0] = cl::Buffer(c, fs, buffer_size);

            std::vector<Rv> buff(v*n_elems);
            for (size_t i=0; i<n_elems; ++i)
            {
#ifdef USE_VECTOR_TYPE
               buff[i] = Rv{xs[i], ys[i], zs[i]};
#else
               buff[3*i+0] = T(xs[i]);
               buff[3*i+1] = T(ys[i]);
               buff[3*i+2] = T(zs[i]);
#endif
            }

            q.enqueueWriteBuffer(buff_container[0], CL_TRUE, 0, buffer_size, &buff[0]);
         }

         // reads data from device, assumes host vectors already have enough capacity
         template <typename R>
         void copy_to_host(const cl::CommandQueue &q,
                           std::vector<R> &xs,
                           std::vector<R> &ys,
                           std::vector<R> &zs) const noexcept
         {
            std::vector<Rv> buff(v*n_elems);
            q.enqueueReadBuffer(buff_container[0], CL_TRUE, 0, buffer_size, &buff[0]);

            for (size_t i=0; i<n_elems; ++i)
            {
#ifdef USE_VECTOR_TYPE
               xs[i] = R(buff[i].s[0]);
               ys[i] = R(buff[i].s[1]);
               zs[i] = R(buff[i].s[2]);
#else
               xs[i] = R(buff[3*i+0]);
               ys[i] = R(buff[3*i+1]);
               zs[i] = R(buff[3*i+2]);
#endif
            }
         }

         // copies buffer to dst buffer on device
         void copy_to_dev(const cl::CommandQueue &q,
                          Buffer3D &dst) const noexcept
         {
            q.enqueueCopyBuffer(buff_container[0], dst.buff_container[0], 0, 0, buffer_size);
            q.finish();
         }

         // writes zeros over the data
         void zero_buffer(void) noexcept
         {
#ifdef CL_API_SUFFIX__VERSION_1_2
            const Rv zero{0.0};
            vcl::queue.enqueueFillBuffer(buff_container[0], &zero, sizeof(Rv), v*n_elems);
#else
            const std::vector<Rv> zeros(3*n_elems, {0.0});
            vcl::queue.enqueueWriteBuffer(buff_container[0], CL_FALSE, 0, buffer_size, &zeros[0]);
#endif // CL_API_SUFFIX__VERSION_1_2

            vcl::queue.finish();
         }

         // access the buffer (to pass to kernels)
         cl::Buffer &buffer(void) noexcept { return buff_container[0]; }

         // release memory by swapping with empty vector
         void free(void) noexcept
         {
            std::vector<cl::Buffer>().swap(buff_container);
         }
      };

   }
}

#endif // VOPENCL_TYPEDEFS_HPP_
