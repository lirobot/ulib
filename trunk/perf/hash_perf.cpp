/* The MIT License

   Copyright (C) 2012 Zilong Tan (labytan@gmail.com)

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <google/sparse_hash_map>
#include <google/dense_hash_map>
#include <google/malloc_extension.h>
#include <ext/hash_map>
#include <ulib/timer.h>

// Random Number Generator
#include <ulib/rand_tpl.h>

// NOTE: define AH_TIER_PROBING when keys are NOT random
// However, undefine it increase the performance
#define AH_TIER_PROBING
#include <ulib/alignhash.h>

using google::sparse_hash_map;
using google::dense_hash_map;
using __gnu_cxx::hash_map;

class key_generator {
public:
	virtual size_t
	operator()() = 0;
};

// We use a high quality RNG which is documented in Numeric Recipies.
class random_key_generator : public key_generator {
public:
	virtual size_t
	operator()()
	{ return RAND_NR_NEXT(_u, _v, _w); }

	random_key_generator(uint64_t seed = 0)
	{
		_seed = time(NULL) ^ seed;
		RAND_NR_INIT(_u, _v, _w, _seed);
	}

private:
	// RNG context
	uint64_t _u, _v, _w;
	uint64_t _seed;
};

class sequential_key_generator : public key_generator {
public:
	virtual size_t
	operator()()
	{ return _counter++; }

	sequential_key_generator(uint64_t seed = 0)
	{ _counter = seed; }

private:
	uint64_t _counter;
};

// The method is provided by time_hash_map.cc in sparsehash, requires gperftools
static size_t current_mem_usage() {
	size_t result;
	if (MallocExtension::instance()->GetNumericProperty(
		    "generic.current_allocated_bytes",
		    &result)) {
		return result;
	}
	return 0;
}

// measure time used for each find() operation
template<class T>
static double
measure_find_time(size_t capacity, size_t loop, key_generator *kg, size_t *mem)
{
	T map;
	struct timespec timer;

	while (capacity-- > 0)
		map[(*kg)()] = 0;

	timer_start(&timer);
	for (size_t i = 0; i < loop; ++i) 
		map.find((*kg)());

	*mem = current_mem_usage();

	return timer_stop(&timer) / loop * 1000000000;
}

// measure time used for each insert() operation
template<class T>
static double
measure_insert_time(size_t capacity, key_generator *kg, size_t *mem)
{
	T map;
	struct timespec timer;

	timer_start(&timer);
	for (size_t i = 0; i < capacity; ++i)
		map[(*kg)()] = 0;

	*mem = current_mem_usage();

	return timer_stop(&timer) / capacity * 1000000000;
}

template<class Key, class Val>
class EasyUseDenseHashMap : public dense_hash_map<Key, Val> {
public:
	EasyUseDenseHashMap()
	{ this->set_empty_key(0); }
};

int main(int argc, char *argv[])
{
	size_t mem;
	size_t capacity = 50000;
	size_t loop     = 1000000;

	sequential_key_generator skg;
	random_key_generator rkg;

	if (argc > 1)
		capacity = strtoul(argv[1], 0, 10);
	if (argc > 2)
		loop = strtoul(argv[2], 0, 10);

	printf("Running with CAPACITY=%lu, LOOP=%lu\n", capacity, loop);
	printf("\n>>>>>>>>>> Insertion:\n\n");
	printf("[Sparse Hash Map] Sequential:%.2f ns\tRandom:%.2f ns\tMemory:%lu\n",
	       measure_insert_time<sparse_hash_map<uint64_t,uint64_t> >(capacity, &skg, &mem),
	       measure_insert_time<sparse_hash_map<uint64_t,uint64_t> >(capacity, &rkg, &mem), mem);
	printf("[STL Hash Map]    Sequential:%.2f ns\tRandom:%.2f ns\tMemory:%lu\n",
	       measure_insert_time<hash_map<uint64_t,uint64_t> >(capacity, &skg, &mem),
	       measure_insert_time<hash_map<uint64_t,uint64_t> >(capacity, &rkg, &mem), mem);
	printf("[Dense Hash Map]  Sequential:%.2f ns\tRandom:%.2f ns\tMemory:%lu\n",
	       measure_insert_time<EasyUseDenseHashMap<uint64_t,uint64_t> >(capacity, &skg, &mem),
	       measure_insert_time<EasyUseDenseHashMap<uint64_t,uint64_t> >(capacity, &rkg, &mem), mem);
	printf("[Align Hash Map]  Sequential:%.2f ns\tRandom:%.2f ns\tMemory:%lu\n",
	       measure_insert_time<align_hash_map<uint64_t,uint64_t> >(capacity, &skg, &mem),
	       measure_insert_time<align_hash_map<uint64_t,uint64_t> >(capacity, &rkg, &mem), mem);

	printf("\n>>>>>>>>>> Search:\n\n");
	printf("[Sparse Hash Map] Sequential:%.2f ns\tRandom:%.2f ns\tMemory:%lu\n",
	       measure_find_time<sparse_hash_map<uint64_t,uint64_t> >(capacity, loop, &skg, &mem),
	       measure_find_time<sparse_hash_map<uint64_t,uint64_t> >(capacity, loop, &rkg, &mem), mem);
	printf("[STL Hash Map]    Sequential:%.2f ns\tRandom:%.2f ns\tMemory:%lu\n",
	       measure_find_time<hash_map<uint64_t,uint64_t> >(capacity, loop, &skg, &mem),
	       measure_find_time<hash_map<uint64_t,uint64_t> >(capacity, loop, &rkg, &mem), mem);
	printf("[Dense Hash Map]  Sequential:%.2f ns\tRandom:%.2f ns\tMemory:%lu\n",
	       measure_find_time<EasyUseDenseHashMap<uint64_t,uint64_t> >(capacity, loop, &skg, &mem),
	       measure_find_time<EasyUseDenseHashMap<uint64_t,uint64_t> >(capacity, loop, &rkg, &mem), mem);
	printf("[Align Hash Map]  Sequential:%.2f ns\tRandom:%.2f ns\tMemory:%lu\n",
	       measure_find_time<align_hash_map<uint64_t,uint64_t> >(capacity, loop, &skg, &mem),
	       measure_find_time<align_hash_map<uint64_t,uint64_t> >(capacity, loop, &rkg, &mem), mem);

	return 0;
}
