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


	// functional_mutex

	functional_mutex::functional_mutex()
	{
		l = false;
	}

	functional_mutex::~functional_mutex()
	{
	}

	void functional_mutex::lock()
	{
		while(!__sync_bool_compare_and_swap(&l,false,true)){
			usleep(10);
		}
	}

	void functional_mutex::unlock()
	{
		l = false;
	}

	bool functional_mutex::try_lock()
	{
		return __sync_bool_compare_and_swap(&l,false,true);
	}
	bool functional_mutex::try_del()
	{
		unlock();
		return false;
	}

}}
