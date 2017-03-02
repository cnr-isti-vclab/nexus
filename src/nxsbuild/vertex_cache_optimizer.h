#ifndef VMATH_GRAPHICS_UTIL_VERTEX_CACHE_OPTIMIZER_H
#define VMATH_GRAPHICS_UTIL_VERTEX_CACHE_OPTIMIZER_H

#include <vector>

namespace vmath
{

class vertex_cache_optimizer
{
	public:

		typedef vertex_cache_optimizer this_type;

		template <typename t_vertex_index, typename t_vertex>
		static inline bool optimize_pre_tnl(const t_vertex_index * in_triangles_indices, int triangles_count, const t_vertex * in_vertices, int vertices_count, t_vertex_index * out_triangles_indices, t_vertex * out_vertices);

		template <typename t_vertex_index>
		static inline bool optimize_post_tnl(int vertex_cache_size, const t_vertex_index * in_triangles_indices, int triangles_count, int vertices_count, t_vertex_index * out_triangles_indices, t_vertex_index * out_indices_permutation = 0);

	protected:

		template <typename t_vertex_adjacency, typename t_vertex_index>
		static inline bool do_optimize_post_tnl(int vertex_cache_size, const t_vertex_index * in_triangles_indices, int triangles_count, int vertices_count, t_vertex_index * out_triangles_indices, t_vertex_index * out_indices_permutation = 0);


		template <typename t_vertex_adjacency, typename t_vertex_index>
		static inline int get_next_vertex(int vertices_count, int & i, int vertex_cache_size, const t_vertex_index * next_candidates, int num_next_candidates, const int * cache_time, int s, const t_vertex_adjacency * live_triangles, const t_vertex_index * dead_end_stack, int & dead_end_stack_pos, int & dead_end_stack_start);

		template <typename t_vertex_adjacency, typename t_vertex_index>
		static inline int skip_dead_end(const t_vertex_adjacency * live_triangles, const t_vertex_index * dead_end_stack, int & dead_end_stack_pos, int & dead_end_stack_start, int vertices_count, int & i);
};



#define VCO_USE_MEMCPY                    (1)
#define VCO_VERTEX_ADJACENCY_MAX_255      (0)

#define VCO_VERTEX_ADJACENCY_CHECK_255    (0)
#define VCO_DEAD_END_STACK_SIZE_LOG2      (10)

/*****************************************************************************/
#if defined(VCO_USE_MEMCPY)
#include <memory.h>
#define _VCO_ZERO_MEMORY_(BUFFER, SIZE)    memset((BUFFER), 0, (SIZE))
#else
static inline void _vco_zero_memory_(unsigned char * buffer, int size) { for (int i=0; i<size; ++i) { *buffer++ = ((unsigned char)(0)); } }
#define _VCO_ZERO_MEMORY_(BUFFER, SIZE)    _vco_zero_memory_((BUFFER), (SIZE))
#endif

#if defined(VCO_VERTEX_ADJACENCY_MAX_255)
#define _VCO_VERTEX_ADJACENCY_TYPE_    unsigned char
#else
#define _VCO_VERTEX_ADJACENCY_TYPE_    unsigned int
#endif

#define _VCO_VERTEX_ADJACENCY_MAX_    ((_VCO_VERTEX_ADJACENCY_TYPE_)(-1))

#define _VCO_DEAD_END_STACK_SIZE_     (1 << VCO_DEAD_END_STACK_SIZE_LOG2)
#define _VCO_DEAD_END_STACK_MASK_     (_VCO_DEAD_END_STACK_SIZE_ - 1)

#define _VCO_IS_EMITTED_(X)           (emitted[(X) >> 3] &  (1 << ((X) & 7)))
#define _VCO_SET_EMITTED_(X)          (emitted[(X) >> 3] |= (1 << ((X) & 7)))
/*****************************************************************************/

template <typename t_vertex_index, typename t_vertex>
inline bool vertex_cache_optimizer::optimize_pre_tnl(const t_vertex_index * in_triangles_indices, int triangles_count, const t_vertex * in_vertices, int vertices_count, t_vertex_index * out_triangles_indices, t_vertex * out_vertices)
{
	typedef t_vertex_index  vertex_index_type;
	typedef t_vertex        vertex_type;

	if (in_triangles_indices  == 0) return false;
	if (triangles_count       <= 0) return false;
	if (in_vertices           == 0) return false;
	if (vertices_count        <= 3) return false;
	if (out_vertices          == 0) return false;

	std::vector<bool>              added_vertices  (size_t(vertices_count), false);
	std::vector<vertex_index_type> remap_indices   ((size_t)(vertices_count));

	const int indices_count = 3 * triangles_count;

	vertex_index_type out_vertices_count = vertex_index_type(0);

	for(int i=0; i<indices_count; ++i)
	{
		const vertex_index_type idx = in_triangles_indices[i];
		if (!added_vertices[idx])
		{
			remap_indices[idx] = out_vertices_count;
			out_vertices[out_vertices_count] = in_vertices[idx];
			out_vertices_count++;
			added_vertices[idx] = true;
		}
		out_triangles_indices[i] = remap_indices[idx];
	}

	return true;
}

template <typename t_vertex_index>
inline bool vertex_cache_optimizer::optimize_post_tnl(int vertex_cache_size, const t_vertex_index * in_triangles_indices, int triangles_count, int vertices_count, t_vertex_index * out_triangles_indices, t_vertex_index * out_indices_permutation)
{
	return this_type::do_optimize_post_tnl<_VCO_VERTEX_ADJACENCY_TYPE_, t_vertex_index>(vertex_cache_size, in_triangles_indices, triangles_count, vertices_count, out_triangles_indices, out_indices_permutation);
}

template <typename t_vertex_adjacency, typename t_vertex_index>
inline bool vertex_cache_optimizer::do_optimize_post_tnl(int vertex_cache_size, const t_vertex_index * in_triangles_indices, int triangles_count, int vertices_count, t_vertex_index * out_triangles_indices, t_vertex_index * out_indices_permutation)
{
	typedef t_vertex_adjacency  adjacency_type;
	typedef t_vertex_index      vertex_index_type;

	if (vertex_cache_size     <= 0) return false;
	if (in_triangles_indices  == 0) return false;
	if (triangles_count       <= 0) return false;
	if (vertices_count        <= 3) return false;
	if (out_triangles_indices == 0) return false;

	adjacency_type * num_occurrences = new adjacency_type [vertices_count];
	_VCO_ZERO_MEMORY_(num_occurrences, sizeof(adjacency_type) * vertices_count);
	for (int i=0; i<(3*triangles_count); ++i)
	{
		const vertex_index_type v = in_triangles_indices[i];
#if (defined(VCO_VERTEX_ADJACENCY_MAX_255) && defined(VCO_VERTEX_ADJACENCY_CHECK_255))
		if (num_occurrences[v] == _VCO_VERTEX_ADJACENCY_MAX_)
		{
			delete [] num_occurrences;
			return false;
		}
#endif
		num_occurrences[v]++;
	}

	int * offsets = new int [vertices_count + 1];
	int sum = 0;
	int max_adjacency = 0;
	for (int i=0; i<vertices_count; ++i)
	{
		offsets[i] = sum;
		sum += num_occurrences[i];
		if (num_occurrences[i] > max_adjacency)
		{
			max_adjacency = num_occurrences[i];
		}
		num_occurrences[i] = 0;
	}
	offsets[vertices_count] = sum;

	int * adjacency = new int [3 * triangles_count];
	for (int i=0; i<triangles_count; ++i)
	{
		const vertex_index_type * vptr = &(in_triangles_indices[3 * i]);
		adjacency[offsets[vptr[0]] + num_occurrences[vptr[0]]] = i;
		num_occurrences[vptr[0]]++;
		adjacency[offsets[vptr[1]] + num_occurrences[vptr[1]]] = i;
		num_occurrences[vptr[1]]++;
		adjacency[offsets[vptr[2]] + num_occurrences[vptr[2]]] = i;
		num_occurrences[vptr[2]]++;
	}

	adjacency_type * live_triangles = num_occurrences;

	int * cache_time = new int [vertices_count];
	_VCO_ZERO_MEMORY_(cache_time, sizeof(int) * vertices_count);

	vertex_index_type * dead_end_stack = new vertex_index_type[_VCO_DEAD_END_STACK_SIZE_];
	_VCO_ZERO_MEMORY_(dead_end_stack, sizeof(vertex_index_type) * _VCO_DEAD_END_STACK_SIZE_);
	int dead_end_stack_pos   = 0;
	int dead_end_stack_start = 0;

	const int emitted_buffer_size = int((triangles_count + 7) / 8);
	unsigned char * emitted = new unsigned char [emitted_buffer_size];
	_VCO_ZERO_MEMORY_(emitted, sizeof(unsigned char) * emitted_buffer_size);

	int * output_triangles = new int [triangles_count];
	int output_pos = 0;

	int f = 0;

	int s = vertex_cache_size + 1;
	int i = 0;

	vertex_index_type * next_candidates = new vertex_index_type [3 * max_adjacency];

	while (f >= 0)
	{
		const int start_offset = offsets[f];
		const int end_offset   = offsets[f + 1];

		int num_next_candidates = 0;

		for (int offset=start_offset; offset<end_offset; ++offset)
		{
			const int t = adjacency[offset];

			if (!_VCO_IS_EMITTED_(t))
			{
				const vertex_index_type * vptr = &(in_triangles_indices[3*t]);

				output_triangles[output_pos++] = t;
				for (int j=0; j<3; ++j)
				{
					const vertex_index_type v = vptr[j];

					dead_end_stack[(dead_end_stack_pos++) & _VCO_DEAD_END_STACK_MASK_] = v;

					if ((dead_end_stack_pos & _VCO_DEAD_END_STACK_MASK_) == (dead_end_stack_start & _VCO_DEAD_END_STACK_MASK_))
					{
						dead_end_stack_start = (dead_end_stack_start + 1) & _VCO_DEAD_END_STACK_MASK_;
					}

					next_candidates[num_next_candidates++] = v;
					live_triangles[v]--;

					if (s - cache_time[v] > vertex_cache_size)
					{
						cache_time[v] = s;
						s++;
					}
				}

				_VCO_SET_EMITTED_(t);
			}
		}

		f = this_type::get_next_vertex<adjacency_type, vertex_index_type>(vertices_count, i, vertex_cache_size, next_candidates, num_next_candidates, cache_time, s, live_triangles, dead_end_stack, dead_end_stack_pos, dead_end_stack_start);
	}

	delete [] next_candidates;
	delete [] emitted;
	delete [] dead_end_stack;
	delete [] cache_time;
	delete [] adjacency;
	delete [] offsets;
	delete [] num_occurrences;

	output_pos = 0;
	if (out_indices_permutation != 0)
	{
		for (int i=0; i<triangles_count; ++i)
		{
			const int t = output_triangles[i];
			for (int j=0; j<3; ++j)
			{
				const int tri_index = 3 * t + j;
				const vertex_index_type v = in_triangles_indices[tri_index];
				out_triangles_indices[output_pos] = v;
				out_indices_permutation[output_pos] = tri_index;
				output_pos++;
			}
		}
	}
	else
	{
		for (int i=0; i<triangles_count; ++i)
		{
			const int t = output_triangles[i];
			for (int j=0; j<3; ++j)
			{
				const vertex_index_type v = in_triangles_indices[3*t + j];
				out_triangles_indices[output_pos++] = v;
			}
		}
	}
	delete [] output_triangles;

	return true;
}

template <typename t_vertex_adjacency, typename t_vertex_index>
inline int vertex_cache_optimizer::get_next_vertex(int vertices_count, int & i, int vertex_cache_size, const t_vertex_index * next_candidates, int num_next_candidates, const int * cache_time, int s, const t_vertex_adjacency * live_triangles, const t_vertex_index * dead_end_stack, int & dead_end_stack_pos, int & dead_end_stack_start)
{
	typedef t_vertex_adjacency  adjacency_type;
	typedef t_vertex_index      vertex_index_type;

	int n = -1;
	int m = -1;
	for (int j=0; j<num_next_candidates; ++j)
	{
		const vertex_index_type v = next_candidates[j];
		if (live_triangles[v] > 0)
		{
			int p = 0;
			if (s - cache_time[v] + int(2 * live_triangles[v]) <= vertex_cache_size)
			{
				p = s - cache_time[v];
			}
			if (p > m)
			{
				m = p;
				n = v;
			}
		}
	}

	if (n == -1)
	{
		n = this_type::skip_dead_end<adjacency_type, vertex_index_type>(live_triangles, dead_end_stack, dead_end_stack_pos, dead_end_stack_start, vertices_count, i);
	}

	return n;
}

template <typename t_vertex_adjacency, typename t_vertex_index>
inline int vertex_cache_optimizer::skip_dead_end(const t_vertex_adjacency * live_triangles, const t_vertex_index * dead_end_stack, int & dead_end_stack_pos, int & dead_end_stack_start, int vertices_count, int & i)
{
	typedef t_vertex_adjacency  adjacency_type;
	typedef t_vertex_index vertex_index_type;

	while ((dead_end_stack_pos & _VCO_DEAD_END_STACK_MASK_) != dead_end_stack_start)
	{
		const vertex_index_type d = dead_end_stack[(--dead_end_stack_pos) & _VCO_DEAD_END_STACK_MASK_];
		if (live_triangles[d] > 0)
		{
			return d;
		}
	}

	while (i + 1 < vertices_count)
	{
		i++;
		if (live_triangles[i] > 0)
		{
			return i;
		}
	}

	return (-1);
}

#undef VCO_USE_MEMCPY
#undef VCO_VERTEX_ADJACENCY_MAX_255
#undef VCO_VERTEX_ADJACENCY_CHECK_255
#undef VCO_DEAD_END_STACK_SIZE_LOG2

#undef _VCO_ZERO_MEMORY_
#undef _VCO_VERTEX_ADJACENCY_TYPE_
#undef _VCO_VERTEX_ADJACENCY_MAX_
#undef _VCO_DEAD_END_STACK_SIZE_
#undef _VCO_DEAD_END_STACK_MASK_
#undef _VCO_IS_EMITTED_
#undef _VCO_SET_EMITTED_

}

#endif // VMATH_GRAPHICS_UTIL_VERTEX_CACHE_OPTIMIZER_H
