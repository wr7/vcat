#include "src/util.hh"
#include <string>

namespace dvel {
	std::string indent(std::string&& input, size_t level) {
		for(size_t i = input.length() - 1;;) {
			if(input[i] == '\n') {
				for(size_t j = 0; j < level; j++) {
					input.insert(i + 1, "  ");
				}
			}

			if(i == 0) {
				break;
			} else {
				i--;
			}
		}

		if(!input.empty()) {
			for(size_t i = 0; i < level; i++) {
				input.insert(0, "  ");
			}
		}

		return std::move(input);
	}
}
