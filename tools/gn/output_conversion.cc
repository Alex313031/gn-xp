// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/output_conversion.h"

#include "tools/gn/settings.h"
#include "tools/gn/value.h"

namespace {

void ToString(const Value& output, std::ostream& out) {
  out << output.ToString(false);
}

void ToStringQuoted(const Value& output, std::ostream& out) {
  out << "\"" << output.ToString(false) << "\"";
}

// Forward declare so it can be used recursively.
void RenderScopeToJSON(const Value& output, std::ostream& out, int indent);

void RenderListToJSON(const Value& output, std::ostream& out, int indent) {
  assert(indent > 0);
  bool first = true;
  out << "[\n";
  for (const auto& value : output.list_value()) {
    if (!first)
      out << ",\n";
    for (int i = 0; i < indent; ++i)
      out << "  ";
    if (value.type() == Value::SCOPE)
      RenderScopeToJSON(value, out, indent + 1);
    else if (value.type() == Value::LIST)
      RenderListToJSON(value, out, indent + 1);
    else
      out << value.ToString(true);
    first = false;
  }
  out << "\n";
  for (int i = 0; i < indent - 1; ++i)
    out << "  ";
  out << "]";
}

void RenderScopeToJSON(const Value& output, std::ostream& out, int indent) {
  assert(indent > 0);
  Scope::KeyValueMap scope_values;
  output.scope_value()->GetCurrentScopeValues(&scope_values);
  bool first = true;
  out << "{\n";
  for (const auto& pair : scope_values) {
    if (!first)
      out << ",\n";
    for (int i = 0; i < indent; ++i)
      out << "  ";
    out << "\"" << pair.first.as_string() << "\": ";
    if (pair.second.type() == Value::SCOPE)
      RenderScopeToJSON(pair.second, out, indent + 1);
    else if (pair.second.type() == Value::LIST)
      RenderListToJSON(pair.second, out, indent + 1);
    else
      out << pair.second.ToString(true);
    first = false;
  }
  out << "\n";
  for (int i = 0; i < indent - 1; ++i)
    out << "  ";
  out << "}";
}

void OutputListLines(const Value& output, std::ostream& out) {
  assert(output.type() == Value::LIST);
  const std::vector<Value>& list = output.list_value();
  for (const auto& cur : list)
    out << cur.ToString(false) << "\n";
}

void OutputString(const Value& output, std::ostream& out) {
  if (output.type() == Value::NONE)
    return;
  if (output.type() == Value::STRING) {
    ToString(output, out);
    return;
  }
  ToStringQuoted(output, out);
}

void OutputValue(const Value& output, std::ostream& out) {
  if (output.type() == Value::NONE)
    return;
  if (output.type() == Value::STRING) {
    ToStringQuoted(output, out);
    return;
  }
  ToString(output, out);
}

// The direct Value::ToString call wraps the scope in '{}', which we don't want
// here for the top-level scope being output.
void OutputScope(const Value& output, std::ostream& out) {
  Scope::KeyValueMap scope_values;
  output.scope_value()->GetCurrentScopeValues(&scope_values);
  for (const auto& pair : scope_values) {
    out << "  " << pair.first.as_string() << " = " << pair.second.ToString(true)
        << "\n";
  }
}

void OutputDefault(const Value& output, std::ostream& out) {
  if (output.type() == Value::LIST)
    OutputListLines(output, out);
  else
    ToString(output, out);
}

void OutputJSON(const Value& output, std::ostream& out) {
  if (output.type() == Value::SCOPE) {
    RenderScopeToJSON(output, out, /*indent=*/1);
    return;
  }
  if (output.type() == Value::LIST) {
    RenderListToJSON(output, out, /*indent=*/1);
    return;
  }
  ToStringQuoted(output, out);
}

void DoConvertValueToOutput(const Value& output,
                            const std::string& output_conversion,
                            const Value& original_output_conversion,
                            std::ostream& out,
                            Err* err) {
  if (output_conversion == "")
    OutputDefault(output, out);
  else if (output_conversion == "list lines")
    OutputListLines(output, out);
  else if (output_conversion == "string")
    OutputString(output, out);
  else if (output_conversion == "value")
    OutputValue(output, out);
  else if (output_conversion == "json")
    OutputJSON(output, out);
  else if (output_conversion == "scope") {
    if (output.type() != Value::SCOPE) {
      *err = Err(original_output_conversion, "Not a valid scope.");
      return;
    }
    OutputScope(output, out);
  } else
    // If we make it here, we didn't match any of the valid options.
    *err = Err(original_output_conversion, "Not a valid output_conversion.",
               "Run gn help output_conversion to see your options.");
}

}  // namespace

const char kOutputConversion_Help[] =
    R"(output_conversion: Specifies how to transform a variable to output.

  output_conversion is an argument to write_file that specifies how the given
  value should be converted into a string for writing.

  Note that if the output Value is empty, the resulting output string
  will be "<void>".

  "" (the default)
      If value is a list, then "list lines"; otherwise "value".

  "list lines"
      Renders the value contents as a list, with a string for each line. The
      newlines will not be present in the result. The last line will end in with
      a newline.

  "string"
      Render the value contents into a single string. The output is:
        a string renders with quotes, e.g. "str"
        an integer renders as a stringified integer, e.g. "6"
        a boolean renders as the associated string, e.g. "true"
        a list renders as a representation of its contents, e.g. "[\"str\", 6]"
        a scope renders as a GN code block of its values. If the Value was:
            Value val;
            val.a = [ "hello.cc", "world.cc" ];
            val.b = 26
          the resulting output would be:
            "{
                a = [ \"hello.cc\", \"world.cc\" ]
                b = 26
            }"

  "value"
      Render the value contents as a literal rvalue. Strings render with escaped
      quotes.

  "scope"
      Render the value contents as a GN code block. If the Value was:
          Value val;
          val.a = [ "hello.cc", "world.cc" ];
          val.b = 26
        the resulting output would be:
          "a = [ \"hello.cc\", \"world.cc\" ]
           b = 26"

  "json"
      Convert the Value to equivalent JSON value. The data
      type mapping is:
        a string in GN maps to a string in JSON
        an integer in GN maps to integer in JSON
        a boolean in GN maps to boolean in JSON
        a list in GN maps to array in JSON
        a scope in GN maps to object in JSON
)";

void ConvertValueToOutput(const Settings* settings,
                          const Value& output,
                          const Value& output_conversion,
                          std::ostream& out,
                          Err* err) {
  if (output_conversion.type() == Value::NONE) {
    OutputDefault(output, out);
    return;
  }
  if (!output_conversion.VerifyTypeIs(Value::STRING, err))
    return;

  DoConvertValueToOutput(output, output_conversion.string_value(),
                         output_conversion, out, err);
}
