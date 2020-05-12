#include <flex_meta_plugin/Tooling.hpp> // IWYU pragma: associated

#include <flexlib/ToolPlugin.hpp>
#include <flexlib/core/errors/errors.hpp>
#include <flexlib/utils.hpp>
#include <flexlib/funcParser.hpp>
#include <flexlib/inputThread.hpp>
#include <flexlib/clangUtils.hpp>
#include <flexlib/clangPipeline.hpp>
#include <flexlib/annotation_parser.hpp>
#include <flexlib/annotation_match_handler.hpp>
#include <flexlib/matchers/annotation_matcher.hpp>
#include <flexlib/options/ctp/options.hpp>
#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp>
#endif // CLING_IS_ON

#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <base/cpu.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/debug/alias.h>
#include <base/debug/stack_trace.h>
#include <base/memory/ptr_util.h>
#include <base/sequenced_task_runner.h>
#include <base/strings/string_util.h>
#include <base/trace_event/trace_event.h>

namespace plugin {

namespace {

static const std::string kGenAttrToken = "{gen};{attr};";
static const std::string kAttrReflectableFlag = "reflectable";

// parse declaration and find all annotations
// that start with kGenAttrToken
// return true if declaration is marked with
// "reflectable" attriblute i.e.
// __attribute__((annotate("{gen};{attr};reflectable;")))
static const bool isReflectable(clang::DeclaratorDecl* decl)
{
  bool res = false;
  if ( auto annotate = decl->getAttr<clang::AnnotateAttr>() )
  {
    std::string annotationCode =
      annotate->getAnnotation().str();
    VLOG(9)
      << "annotation code: "
      << annotationCode;
    const bool startsWithGen =
      annotationCode.rfind(kGenAttrToken, 0) == 0;
    annotationCode.erase(0, kGenAttrToken.size());
    if (startsWithGen) {
      std::string delimiter = ";";
      size_t pos = 0;
      std::string token;
      while ((pos = annotationCode.find(delimiter)) != std::string::npos) {
        token = annotationCode.substr(0, pos);
        VLOG(9)
          << "isReflectable token "
          << token;
        if(token == kAttrReflectableFlag) {
          res = true;
          break;
        }
        annotationCode.erase(0, pos + delimiter.length());
      }
      if(!annotationCode.empty()
         && annotationCode == kAttrReflectableFlag) {
        res = true;
      }
    }
  }

  VLOG(9)
    << "isReflectable attr() "
    << decl->getNameAsString()
    << "is " << res;

  return res;
}

} // namespace

Tooling::Tooling(
#if defined(CLING_IS_ON)
  ::cling_utils::ClingInterpreter* clingInterpreter
#endif // CLING_IS_ON
) : clingInterpreter_(clingInterpreter)
{
  DCHECK(clingInterpreter_);

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Tooling::~Tooling()
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

clang_utils::SourceTransformResult
  Tooling::make_reflect(
    const clang_utils::SourceTransformOptions& sourceTransformOptions)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(9)
    << "make_removefuncbody called...";

  std::string indent = "  ";
  std::string output{};

  /// \note For simplisity we didn't use template engine.
  /// You can integrate with any template engine
  /// to avoid code like `output.append("\n");`
  output.append("\n");
  output.append(indent
                  + "public:");
  indent.append("  ");
  output.append("\n");

  std::map<std::string, std::string> fields;
  std::map<std::string, std::string> methods;

  // used annotation attribute
  // must point to
  // __attribute__((annotate("{gen};{funccall};make_reflect;...")))
  clang::CXXRecordDecl const *record =
      sourceTransformOptions.matchResult.Nodes
      .getNodeAs<clang::CXXRecordDecl>("bind_gen");

  if (record) {
    VLOG(9)
      << "record name is "
      << record->getNameAsString().c_str();

    // see https://github.com/Papierkorb/bindgen/blob/b55578e517a308778f5a510de02af499b353f15d/clang/src/record_match_handler.cpp
    for (clang::Decl *decl : record->decls()) {
      if (clang::CXXMethodDecl *method
            = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
        //runOnMethod(method, isSignal);
        DLOG(INFO) << "reflect is CXXMethodDecl " <<
          method->getNameInfo().getName().getAsString().c_str() << " " <<
          method->getReturnType().getAsString().c_str() << " " <<
          method->getType().getUnqualifiedType().getAsString().c_str() << " " <<
          method->getNameAsString().c_str();
        if(isReflectable(method)) {
          methods[method->getNameInfo().getName().getAsString()] =
            method->getReturnType().getAsString().c_str();
        }
      } else if (clang::AccessSpecDecl *spec
                    = llvm::dyn_cast<clang::AccessSpecDecl>(decl)) {
        //isSignal = AccessSpecDecl(spec);
        VLOG(9)
          << ("is CXXMethodDecl %s\n", spec->getNameAsString().c_str());
      } else if (clang::FieldDecl *field
                    = llvm::dyn_cast<clang::FieldDecl>(decl)) {
        VLOG(9)
          << "field type is"
          << field->getType().getUnqualifiedType().getAsString().c_str()
          << " and field name is "
          << field->getNameAsString().c_str();
        if(isReflectable(field)) {
          fields[field->getNameAsString()] =
            field->getType().getUnqualifiedType().getAsString().c_str();
        }
      }
    }

    // TODO: use template

    output.append(indent
                    + "static std::map<std::string, std::string> fields");
    output.append(" = {");
    output.append("\n");
    for(const auto& [key, value] : fields) {
      output.append(indent
                      + indent + "{ ");
      output.append("\"" + key + "\"");
      output.append(", ");
      output.append("\"" + value + "\"");
      output.append(" }");
      output.append("\n");
    }
    output.append("\n");
    output.append(indent
                  + "};");
    output.append("\n");
    // methods
    output.append("\n");
    output.append(indent
                    + "static std::map<std::string, std::string> methods");
    output.append(" = {");
    output.append("\n");
    for(const auto& [key, value] : methods) {
      output.append(indent + indent
                      + "{ ");
      output.append("\"" + key + "\"");
      output.append(", ");
      output.append("\"" + value + "\"");
      output.append(" }");
      output.append("\n");
    }
    output.append("\n");
    output.append(indent +
                    "};");
    output.append("\n");
    auto locEnd = record->getLocEnd();

    // add new field with reflection data at the end of the C++ record
    sourceTransformOptions.rewriter.InsertText(locEnd, output,
      /*InsertAfter=*/true, /*IndentNewLines*/ false);
  }
  return clang_utils::SourceTransformResult{nullptr};
}

} // namespace plugin
