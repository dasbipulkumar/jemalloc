#include "test/jemalloc_test.h"

TEST_BEGIN(test_small_extent_size)
{
	unsigned nbins, i;
	size_t sz, extent_size;
	size_t mib[4];
	size_t miblen = sizeof(mib) / sizeof(size_t);

	/*
	 * Iterate over all small size classes, get their extent sizes, and
	 * verify that the quantized size is the same as the extent size.
	 */

	sz = sizeof(unsigned);
	assert_d_eq(mallctl("arenas.nbins", &nbins, &sz, NULL, 0), 0,
	    "Unexpected mallctl failure");

	assert_d_eq(mallctlnametomib("arenas.bin.0.run_size", mib, &miblen), 0,
	    "Unexpected mallctlnametomib failure");
	for (i = 0; i < nbins; i++) {
		mib[2] = i;
		sz = sizeof(size_t);
		assert_d_eq(mallctlbymib(mib, miblen, &extent_size, &sz, NULL,
		    0), 0, "Unexpected mallctlbymib failure");
		assert_zu_eq(extent_size,
		    extent_size_quantize_floor(extent_size),
		    "Small extent quantization should be a no-op "
		    "(extent_size=%zu)", extent_size);
		assert_zu_eq(extent_size,
		    extent_size_quantize_ceil(extent_size),
		    "Small extent quantization should be a no-op "
		    "(extent_size=%zu)", extent_size);
	}
}
TEST_END

TEST_BEGIN(test_huge_extent_size)
{
	bool cache_oblivious;
	unsigned nhchunks, i;
	size_t sz, extent_size_prev, ceil_prev;
	size_t mib[4];
	size_t miblen = sizeof(mib) / sizeof(size_t);

	/*
	 * Iterate over all huge size classes, get their extent sizes, and
	 * verify that the quantized size is the same as the extent size.
	 */

	sz = sizeof(bool);
	assert_d_eq(mallctl("config.cache_oblivious", &cache_oblivious, &sz,
	    NULL, 0), 0, "Unexpected mallctl failure");

	sz = sizeof(unsigned);
	assert_d_eq(mallctl("arenas.nhchunks", &nhchunks, &sz, NULL, 0), 0,
	    "Unexpected mallctl failure");

	assert_d_eq(mallctlnametomib("arenas.hchunk.0.size", mib, &miblen), 0,
	    "Unexpected mallctlnametomib failure");
	for (i = 0; i < nhchunks; i++) {
		size_t lextent_size, extent_size, floor, ceil;

		mib[2] = i;
		sz = sizeof(size_t);
		assert_d_eq(mallctlbymib(mib, miblen, &lextent_size, &sz, NULL,
		    0), 0, "Unexpected mallctlbymib failure");
		extent_size = cache_oblivious ? lextent_size + PAGE :
		    lextent_size;
		floor = extent_size_quantize_floor(extent_size);
		ceil = extent_size_quantize_ceil(extent_size);

		assert_zu_eq(extent_size, floor,
		    "Large run quantization should be a no-op for precise "
		    "size (lextent_size=%zu, extent_size=%zu)", lextent_size,
		    extent_size);
		assert_zu_eq(extent_size, ceil,
		    "Large run quantization should be a no-op for precise "
		    "size (lextent_size=%zu, extent_size=%zu)", lextent_size,
		    extent_size);

		if (i > 0) {
			assert_zu_eq(extent_size_prev,
			    extent_size_quantize_floor(extent_size - PAGE),
			    "Floor should be a precise size");
			if (extent_size_prev < ceil_prev) {
				assert_zu_eq(ceil_prev, extent_size,
				    "Ceiling should be a precise size "
				    "(extent_size_prev=%zu, ceil_prev=%zu, "
				    "extent_size=%zu)", extent_size_prev,
				    ceil_prev, extent_size);
			}
		}
		if (i + 1 < nhchunks) {
			extent_size_prev = floor;
			ceil_prev = extent_size_quantize_ceil(extent_size +
			    PAGE);
		}
	}
}
TEST_END

TEST_BEGIN(test_monotonic)
{
#define	SZ_MAX	ZU(4 * 1024 * 1024)
	unsigned i;
	size_t floor_prev, ceil_prev;

	floor_prev = 0;
	ceil_prev = 0;
	for (i = 1; i <= SZ_MAX >> LG_PAGE; i++) {
		size_t extent_size, floor, ceil;

		extent_size = i << LG_PAGE;
		floor = extent_size_quantize_floor(extent_size);
		ceil = extent_size_quantize_ceil(extent_size);

		assert_zu_le(floor, extent_size,
		    "Floor should be <= (floor=%zu, extent_size=%zu, ceil=%zu)",
		    floor, extent_size, ceil);
		assert_zu_ge(ceil, extent_size,
		    "Ceiling should be >= (floor=%zu, extent_size=%zu, "
		    "ceil=%zu)", floor, extent_size, ceil);

		assert_zu_le(floor_prev, floor, "Floor should be monotonic "
		    "(floor_prev=%zu, floor=%zu, extent_size=%zu, ceil=%zu)",
		    floor_prev, floor, extent_size, ceil);
		assert_zu_le(ceil_prev, ceil, "Ceiling should be monotonic "
		    "(floor=%zu, extent_size=%zu, ceil_prev=%zu, ceil=%zu)",
		    floor, extent_size, ceil_prev, ceil);

		floor_prev = floor;
		ceil_prev = ceil;
	}
}
TEST_END

int
main(void)
{

	return (test(
	    test_small_extent_size,
	    test_huge_extent_size,
	    test_monotonic));
}