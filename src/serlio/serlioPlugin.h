#pragma once

#ifdef _WIN32
#	ifdef SRL_TEST_EXPORTS
#		define SRL_TEST_EXPORTS_API __declspec(dllexport)
#	else
#		define SRL_TEST_EXPORTS_API
#	endif
#else
#	define SRL_TEST_EXPORTS_API __attribute__ ((visibility ("default")))
#endif
