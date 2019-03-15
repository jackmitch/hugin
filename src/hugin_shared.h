
#if defined _WIN32 && defined Hugin_shared 

#if defined huginbase_EXPORTS
#define IMPEX __declspec(dllexport)
#else
#define IMPEX __declspec(dllimport)
#endif

#if defined celeste_EXPORTS
#define CELESTEIMPEX __declspec(dllexport)
#else
#define CELESTEIMPEX __declspec(dllimport)
#endif

#if defined makefilelib_EXPORTS
#define MAKEIMPEX __declspec(dllexport)
#else
#define MAKEIMPEX __declspec(dllimport)
#endif

#if defined icpfindlib_EXPORTS
#define ICPIMPEX __declspec(dllexport)
#else
#define ICPIMPEX __declspec(dllimport)
#endif

#if defined huginbasewx_EXPORTS
#define WXIMPEX __declspec(dllexport)
#else
#define WXIMPEX __declspec(dllimport)
#endif

#if defined localfeatures_EXPORTS
#define LFIMPEX __declspec(dllexport)
#else
#define LFIMPEX __declspec(dllimport)
#endif

#ifdef _MSC_VER
#pragma warning( disable: 4251 )
#endif

#else
#define IMPEX
#define WXIMPEX
#define MAKEIMPEX
#define LFIMPEX
#define ICPIMPEX
#define CELESTEIMPEX
#define LINESIMPEX
#endif
