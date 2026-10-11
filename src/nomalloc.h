#pragma once

#ifdef malloc
    #undef malloc
#endif

#ifdef calloc
    #undef calloc
#endif

#ifdef realloc
    #undef realloc
#endif

#ifdef free
    #undef free
#endif

#ifdef strdup
    #undef strdup
#endif

#ifdef _strdup
    #undef _strdup
#endif

#ifdef aligned_alloc
    #undef aligned_alloc
#endif

#ifdef memalign
    #undef memalign
#endif

#ifdef posix_memalign
    #undef posix_memalign
#endif

#ifdef _aligned_malloc
    #undef _aligned_malloc
#endif

#ifdef _aligned_realloc
    #undef _aligned_realloc
#endif

#ifdef _aligned_recalloc
    #undef _aligned_recalloc
#endif

#ifdef _aligned_msize
    #undef _aligned_msize
#endif

#ifdef _aligned_free
    #undef _aligned_free
#endif
