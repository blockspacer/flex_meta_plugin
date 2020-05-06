#include <flexlib/ToolPlugin.hpp>

#include "flexlib/core/errors/errors.hpp"

//#include "ctp_scripts/1_utils/CXTPL_STD/CXTPL_STD.hpp"
//#include "ctp_scripts/1_utils/CXXCTP_STD/CXXCTP_STD.hpp"

#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "flexlib/utils.hpp"

#include "flexlib/funcParser.hpp"
#include "flexlib/inputThread.hpp"

#include "flexlib/clangUtils.hpp"

#include "flexlib/clangPipeline.hpp"

#include "flexlib/annotation_parser.hpp"
#include "flexlib/annotation_match_handler.hpp"

#include "flexlib/matchers/annotation_matcher.hpp"

#include "flexlib/options/ctp/options.hpp"

#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON

//#include "flexlib/ctp_registry.hpp"

#include <base/logging.h>
#include <base/cpu.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/debug/alias.h>
#include <base/debug/stack_trace.h>
#include <base/memory/ptr_util.h>
#include <base/sequenced_task_runner.h>
#include <base/strings/string_util.h>
#include <base/trace_event/trace_event.h>

static const std::string kPluginDebugLogName = "(AnnotationPipeline plugin)";

static const std::string kVersion = "v0.0.1";

static const std::string kVersionCommand = "/version";

#if !defined(APPLICATION_BUILD_TYPE)
#define APPLICATION_BUILD_TYPE "local build"
#endif

namespace plugin {

class AnnotationPipeline
  final
  : public ::plugin::ToolPlugin {
 public:
  explicit AnnotationPipeline(
    ::plugin::AbstractManager& manager
    , const std::string& plugin)
    : ::plugin::ToolPlugin{manager, plugin}
  {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  std::string title() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("title");
  }

  std::string author() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("author");
  }

  std::string description() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("description");
  }

  bool load() override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationPipeline::load()");

    DLOG(INFO)
      << "loaded plugin with title = "
      << title()
      << " and description = "
      << description().substr(0, 100)
      << "...";

    return true;
  }

  void disconnect_dispatcher(
    entt::dispatcher &event_dispatcher) override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationPipeline::disconnect_dispatcher()");

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::StringCommand>()
        .disconnect<
          &AnnotationPipeline::handle_event_StringCommand>(this);

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterAnnotationMethods>()
        .disconnect<
          &AnnotationPipeline::handle_event_RegisterAnnotationMethods>(this);

#if defined(CLING_IS_ON)
    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterClingInterpreter>()
        .disconnect<
          &AnnotationPipeline::handle_event_RegisterClingInterpreter>(this);
#endif // CLING_IS_ON
  }

  void connect_to_dispatcher(
    entt::dispatcher &event_dispatcher) override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationPipeline::connect_to_dispatcher()");

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::StringCommand>()
        .connect<
          &AnnotationPipeline::handle_event_StringCommand>(this);

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterAnnotationMethods>()
        .connect<
          &AnnotationPipeline::handle_event_RegisterAnnotationMethods>(this);

#if defined(CLING_IS_ON)
    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterClingInterpreter>()
        .connect<
          &AnnotationPipeline::handle_event_RegisterClingInterpreter>(this);
#endif // CLING_IS_ON
  }

  void handle_event_StringCommand(
    const ::plugin::ToolPlugin::Events::StringCommand& event)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationPipeline::handle_event(StringCommand)");

    if(event.split_parts.size() == 1)
    {
      if(event.split_parts[0] == kVersionCommand) {
        LOG(INFO)
          << kPluginDebugLogName
          << " application version: "
          << kVersion;
        LOG(INFO)
          << kPluginDebugLogName
          << " application build type: "
          << APPLICATION_BUILD_TYPE;
      }
    }
  }

#if defined(CLING_IS_ON)
  void handle_event_RegisterClingInterpreter(
    const ::plugin::ToolPlugin::Events::RegisterClingInterpreter& event)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationPipeline::handle_event(RegisterClingInterpreter)");

    DCHECK(event.clingInterpreter);
    clingInterpreter_ = event.clingInterpreter;
  }
#endif // CLING_IS_ON

  void handle_event_RegisterAnnotationMethods(
    const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationPipeline::handle_event(RegisterAnnotationMethods)");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    DCHECK(event.sourceTransformPipeline);
    ::clang_utils::SourceTransformPipeline& sourceTransformPipeline
      = *event.sourceTransformPipeline;

    ::clang_utils::SourceTransformRules& sourceTransformRules
      = sourceTransformPipeline.sourceTransformRules;
    
    sourceTransformRules["make_reflect"] =
      base::BindRepeating(
        &AnnotationPipeline::make_reflect,
        base::Unretained(this));
  }

  cxxctp_callback_result make_reflect(
    const cxxctp_callback_args& callback_args)
  {
    DLOG(INFO) << "make_removefuncbody called...";

    std::string indent = "  ";
    std::string output{};
    output.append("\n");
    output.append(indent
                    + "public:");
    indent.append("  ");
    output.append("\n");

    std::map<std::string, std::string> fields;
    std::map<std::string, std::string> methods;

    clang::CXXRecordDecl const *record =
        callback_args.matchResult.Nodes.getNodeAs<clang::CXXRecordDecl>("bind_gen");
    if (record) {
      DLOG(INFO) << "reflect is record " << record->getNameAsString().c_str();

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
          //DLOG(INFO) << ("is CXXMethodDecl %s\n", spec->getNameAsString().c_str());
        } else if (clang::FieldDecl *field
                      = llvm::dyn_cast<clang::FieldDecl>(decl)) {
          //runOnField(field);
          DLOG(INFO) << "reflect is FieldDecl" <<
            field->getType().getUnqualifiedType().getAsString().c_str() << " " <<
            field->getNameAsString().c_str();
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
      callback_args.rewriter.InsertText(locEnd, output,
        /*InsertAfter=*/true, /*IndentNewLines*/ false);
    }
    return cxxctp_callback_result{""};
  }

  bool unload() override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationPipeline::unload()");

    DLOG(INFO)
      << "unloaded plugin with title = "
      << title()
      << " and description = "
      << description().substr(0, 100)
      << "...";

    return true;
  }

private:

#if defined(CLING_IS_ON)
  ::cling_utils::ClingInterpreter* clingInterpreter_;
#endif // CLING_IS_ON

  DISALLOW_COPY_AND_ASSIGN(AnnotationPipeline);
};

} // namespace plugin

REGISTER_PLUGIN(/*name*/ AnnotationPipeline
    , /*className*/ plugin::AnnotationPipeline
    // plugin interface version checks to avoid unexpected behavior
    , /*interface*/ "backend.ToolPlugin/1.0")
