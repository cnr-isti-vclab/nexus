#pragma once

#include <iostream>

namespace nx {

enum class Verbosity {
	Silent = 0,
	Default = 1,
	Verbose = 2
};

class LogStream {
public:
	explicit LogStream(Verbosity level) : level_(level) {}

	template <typename T>
	LogStream& operator<<(const T& value) {
		if (shouldLog()) {
			(*stream_) << value;
		}
		return *this;
	}

	LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
		if (shouldLog()) {
			(*stream_) << manip;
		}
		return *this;
	}

	static void setVerbosity(Verbosity verbosity) {
		verbosity_ = verbosity;
	}

	static Verbosity getVerbosity() {
		return verbosity_;
	}

private:
	

	bool shouldLog() const {
		return verbosity_ >= level_;
	}

	Verbosity level_;
	static inline Verbosity verbosity_ = Verbosity::Default;
	static inline std::ostream* stream_ = &std::cout;
};

inline LogStream log(Verbosity::Default);
inline LogStream debug(Verbosity::Verbose);

inline void setVerbosity(Verbosity verbosity) {
	LogStream::setVerbosity(verbosity);
}

} // namespace nx
