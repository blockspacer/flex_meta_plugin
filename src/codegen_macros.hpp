#pragma once

/// \note You can conditionally disable some macro using:
/// defined(CLANG_ENABLED) || defined(CLING_ENABLED)
/// \todo use defined(...) to create executeCodeLineAndKeepInSourceFile

// \note if you don't want to keep trailing ';'
// then place __attribute__ near namespace like below:
/**
// will be replaced with 234432
namespace
$executeCodeAndReplace(
  new llvm::Optional<std::string>{"234432"};
) {}
  **/

/**
 * USAGE:

// will be replaced with empty string
__attribute__((annotate("{gen};{executeCode};\
printf(\"Hello me!\");"))) \
int SOME_UNIQUE_NAME1
;

// will be replaced with empty string
$executeCodeLine(LOG(INFO) << "Hello world!";)
int a0;

// will be replaced with empty string
$executeCodeAndEmptyReplace(
  LOG(INFO) << "Hello me!";
) int a1;

// will be replaced with 1234711
class
$executeCodeAndReplace(
  new llvm::Optional<std::string>{"1234711"};
) {};

// will be replaced with 56
$executeCodeAndReplace(
  new llvm::Optional<std::string>{"56"};
) int a3;

#include <string>
#include <vector>

struct
  $apply(
    make_reflect
  )
SomeStructName {
 public:
  SomeStructName() {
    // ...
  }
 private:
  //__attribute__((annotate("{gen};{attr};reflectable;")))
  const int m_bar2 = 2;

  __attribute__((annotate("{gen};{attr};reflectable;")))
  std::vector<std::string> m_VecStr2;
};
  **/

#define GEN_CAT(a, b) GEN_CAT_I(a, b)
#define GEN_CAT_I(a, b) GEN_CAT_II(~, a ## b)
#define GEN_CAT_II(p, res) res

/**
 * USAGE:
// will be replaced with 123471
$executeCodeAndReplace(
  new llvm::Optional<std::string>{"123471"};
) int GEN_UNIQUE_NAME(__tmp__executeCodeAndReplace);
 **/
#define GEN_UNIQUE_NAME(base) GEN_CAT(base, __COUNTER__)

// executeStringWithoutSpaces executes code
// unlike exec, executeStringWithoutSpaces may use `#include` e.t.c.
// no return value
#define $executeStringWithoutSpaces(...) \
  /* generate definition required to use __attribute__ */ \
  __attribute__((annotate("{gen};{executeStringWithoutSpaces};" __VA_ARGS__)))

// executeCodeAndReplace executes code and
// returns (optional) source code modification
#define $executeCodeAndReplace(...) \
  /* generate definition required to use __attribute__ */ \
  __attribute__((annotate("{gen};{executeCodeAndReplace};" #__VA_ARGS__)))

// shortened executeCodeAndReplace syntax
// param1 - returns (optional) source code modification
#define $executeCodeAndReplaceTo(RETVAL, ...) \
  /* generate definition required to use __attribute__ */ \
  __attribute__((annotate( \
      "{gen};{executeCodeAndReplace};" \
      "[&clangMatchResult, &clangRewriter, &clangDecl]() {" \
        #__VA_ARGS__ \
        "return new llvm::Optional<std::string>{" \
          #RETVAL \
          "};" \
      "}();" \
    )))

// executeStringWithoutSpaces that can accept not quoted code line
#define $executeCodeLine(...) \
  __attribute__((annotate("{gen};{executeStringWithoutSpaces};" #__VA_ARGS__ )))

// exec is similar to executeCodeAndReplace,
// but returns empty source code modification
#define $executeCodeAndEmptyReplace(...) \
  /* generate definition required to use __attribute__ */ \
  __attribute__((annotate( \
      "{gen};{executeCodeAndReplace};" \
      "[&clangMatchResult, &clangRewriter, &clangDecl]() {" \
        #__VA_ARGS__ \
        "return new llvm::Optional<std::string>{\"\"};" \
      "}();" \
    )))

#define $apply(...) \
  __attribute__((annotate("{gen};{funccall};" #__VA_ARGS__)))

