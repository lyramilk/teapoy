#include "functional_impl_cavedb.h"

namespace lyramilk{ namespace teapoy {



	static __attribute__ ((constructor)) void __init()
	{
		//functional_master::instance()->define("redis",new functional_impl_redis);
	}
}}
