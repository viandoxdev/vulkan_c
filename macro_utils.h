#ifndef MACRO_UTILS_H
#define MACRO_UTILS_H

#define CALL(m, ...) m(__VA_ARGS__)

#define EMPTY()

#define EVAL(...) EVAL16(__VA_ARGS__)
#define EVAL1024(...) EVAL512(EVAL512(__VA_ARGS__))
#define EVAL512(...) EVAL256(EVAL256(__VA_ARGS__))
#define EVAL256(...) EVAL128(EVAL128(__VA_ARGS__))
#define EVAL128(...) EVAL64(EVAL64(__VA_ARGS__))
#define EVAL64(...) EVAL32(EVAL32(__VA_ARGS__))
#define EVAL32(...) EVAL16(EVAL16(__VA_ARGS__))
#define EVAL16(...) EVAL8(EVAL8(__VA_ARGS__))
#define EVAL8(...) EVAL4(EVAL4(__VA_ARGS__))
#define EVAL4(...) EVAL2(EVAL2(__VA_ARGS__))
#define EVAL2(...) EVAL1(EVAL1(__VA_ARGS__))
#define EVAL1(...) __VA_ARGS__

#define DEFER1(x) x EMPTY()
#define DEFER2(x) x EMPTY EMPTY()()
#define DEFER3(x) x EMPTY EMPTY EMPTY()()()
#define DEFER4(x) x EMPTY EMPTY EMPTY EMPTY()()()()
#define DEFER5(x) x EMPTY EMPTY EMPTY EMPTY EMPTY()()()()()

#define MAP(m, fst, ...) m(fst) __VA_OPT__(DEFER1(_MAP)()(DEFER1(m), __VA_ARGS__))
#define _MAP() MAP

#endif
