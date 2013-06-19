#ifndef NODEC_MEMORY_H
#define NODEC_MEMORY_H

#include <cstdlib>

namespace nodec {
  namespace mem {

    template <typename T> T* alloc() {
      return static_cast<T*> (std::malloc(sizeof(T)));
    };

    template <typename T> T* calloc(size_t count) {
      return static_cast<T*> (std::calloc(count, sizeof(T)));
    };

  };
};

#endif
