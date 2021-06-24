#ifndef NX_RAM_CACHE_H
#define NX_RAM_CACHE_H

#include <sstream>
#include <iostream>

#include "nexus.h"
#include "token.h"
#include <wrap/gcache/cache.h>
#ifdef USE_CURL
#include <curl/curl.h>
#endif
#include <wrap/system/multithreading/util.h>

size_t nx_curl_write_callback( char *ptr, size_t size, size_t nmemb, void *userdata);
size_t nx_curl_header_callback( char *ptr, size_t size, size_t nmemb, void *userdata);

namespace nx {

struct CurlData {
	CurlData(char *b, uint32_t e): buffer(b), written(0), expected(e) {}
	char *buffer;
	uint32_t written;
	uint32_t expected;
};

class RamCache: public vcg::Cache<nx::Token> {
public:
#ifdef USE_CURL
	CURL *curl;
#else
	void *curl;
#endif

	std::list<Nexus *> loading;
	mt::mutex loading_mutex;

	RamCache(): curl(NULL) { }
	~RamCache() { }

#ifdef USE_CURL
protected:
	void run() {
		curl = curl_easy_init();
		vcg::Cache<nx::Token>::run();
		curl_easy_cleanup(curl);
	}
#endif
	void middle() {
		mt::mutexlocker locker(&loading_mutex);
		if(loading.size()) {
			loadNexus(loading.front());
			loading.pop_front();
		}
	}

public:

	void add(Nexus *nexus) {
		mt::mutexlocker locker(&loading_mutex);
		loading.push_back(nexus);
	}

	void abort() { } //this will be called in another thread!
	void loadNexus(Nexus *nexus);
	int get(nx::Token *in);
	int drop(nx::Token *in) { return in->nexus->dropRam(in->node); }
	int size(nx::Token *in);
	int size() { return Cache<Token>::size(); }

protected:
	uint64_t getCurl(const char *url, CurlData &data, uint64_t start, uint64_t end);
};

}//namespace
#endif // NX_RAM_CACHE_H
