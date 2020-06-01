#include "functional_master.h"
#include <unistd.h>

namespace lyramilk{ namespace teapoy {


	bool functional::try_del()
	{
		return false;
	}

	// functional_master

	functional_master* functional_master::instance()
	{
		static functional_master _mm;
		return &_mm;
	}

	functional_type_master* functional_type_master::instance()
	{
		static functional_type_master _mm;
		return &_mm;
	}


	// functional_nonreentrant

	functional_nonreentrant::functional_nonreentrant()
	{
		l = false;
	}

	functional_nonreentrant::~functional_nonreentrant()
	{
	}

	void functional_nonreentrant::lock()
	{
		while(!__sync_bool_compare_and_swap(&l,false,true)){
			usleep(10);
		}
	}

	void functional_nonreentrant::unlock()
	{
		l = false;
	}

	bool functional_nonreentrant::try_lock()
	{
		return __sync_bool_compare_and_swap(&l,false,true);
	}
	bool functional_nonreentrant::try_del()
	{
		if(payload() > 0) return false;
		unlock();
		return false;
	}


	// functional_multi

	functional_multi::functional_multi()
	{
	}

	functional_multi::~functional_multi()
	{
	}

}}
