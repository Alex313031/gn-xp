// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::{bail, Context};
use autocxx::prelude::*;
use cxx::CxxString;
use starlark::values::dict::DictRef;
use std::cell::RefCell;
use std::env;
use std::ffi::CString;
use std::os::raw::c_char;
use std::os::unix::ffi::OsStrExt;
use std::pin::Pin;
use std::ptr;
use starlark::eval::{Evaluator, ReturnFileLoader};
use starlark::environment::{Module, GlobalsBuilder, FrozenModule,};
use starlark::starlark_module;
use starlark::values::none::NoneType;
use starlark::values::function::NativeFunction;
use starlark::values::list::ListRef;
use starlark::values::{StringValue, Value, AllocValue};
use starlark::syntax::{AstModule, Dialect};
use starlark::any::ProvidesStaticType;

include_cpp! {
    #include "starlark-rs/gn_helpers.h"
    #include "gn/gen.h"
    #include "gn/input_alternate_loader.h"
    #include "gn/input_file.h"
    safety!(unsafe_ffi)
    generate!("GNImportResult")
    generate!("Err")
    generate!("Gen")
    generate!("InputFile")
    generate!("BlockNode")
    generate!("EndNode")
    generate!("BinaryOpNode")
    generate!("FunctionCallNode")
    generate!("ListNode")
    generate!("LiteralNode")
    generate!("IdentifierNode")
    generate!("Location")
    generate!("Token")
    generate!("make_token")
    generate!("upcast_binary_op")
    generate!("upcast_list")
    generate!("make_identifier_node")
    generate!("make_literal_node")
}
#[cxx::bridge]
mod ffi2 {
    extern "Rust" {
        type ParseResult;
        fn exec_starlark(gn_exec_context: UniquePtr<GNExecContext>, starlark_ast: Box<ParseResult>);
        fn parse_starlark(gn_input_file: &InputFile, filename: &CxxString) -> Result<Box<ParseResult>>;
    }
    unsafe extern "C++" {
        include!("starlark-rs/gn_helpers.h");
        include!("starlark-rs/stargn_main.h");
        include!("gn/scope.h");
        type Scope; // autocxx can't handle this
        type Err = crate::ffi::Err;
        type InputFile = crate::ffi::InputFile;
        unsafe fn call_main(argc: i32, argv: *mut *mut c_char) -> i32;
        type FunctionCallNode = crate::ffi::FunctionCallNode;
        type GNImportResult = crate::ffi::GNImportResult;
        type GNExecContext; // autocxx can't handle this (constructor only used in C++ has transitive dep on Scope)
        fn ExecuteImport(self: Pin<&mut GNExecContext>, import: UniquePtr<FunctionCallNode>, err: Pin<&mut Err>) -> UniquePtr<GNImportResult>;
        fn ExecuteTarget(self: Pin<&mut GNExecContext>, function: UniquePtr<FunctionCallNode>, err: Pin<&mut Err>);
    }
    // There are no C++ functions that have std::unique_ptr<GNExecContext> in their signature
    // but we would like to use it. https://github.com/dtolnay/cxx/pull/336
    impl UniquePtr<GNExecContext> {}
}
 
struct ParseResult {
    // TODO: store the InputFile* here so that we can create tokens with it.
    ast_module: AstModule,
}

// Context for a Starlark module's evaluation.
// Needs to provide a static type to save in Evaluator#extra and cast back to EvalContext<'a> later.
#[derive(ProvidesStaticType)]
struct EvalContext {
    // We need to be able to mutate the GN exec context, but Starlark Rust
    // only supports immutable evaluator extra data. Use a RefCell to allow mutates.
    gn_exec_context: RefCell<UniquePtr<ffi2::GNExecContext>>,
}

impl EvalContext {
    fn from_starlark<'a>(eval: &Evaluator<'_, 'a>) -> anyhow::Result<&'a EvalContext> {
        match eval.extra {
            None => panic!("eval.extra is missing"),
            Some(extra) => extra
                .downcast_ref::<EvalContext>()
                .context("eval.extra is not EvalContext")
        }
    }

    fn execute_import(&self, import: UniquePtr<ffi::FunctionCallNode>, err: Pin<&mut ffi::Err>) -> UniquePtr<ffi::GNImportResult> {
        let mut gn_context = self.gn_exec_context.borrow_mut();
        gn_context.pin_mut().ExecuteImport(import, err)
    }

    fn execute_target(&self, function: UniquePtr<ffi::FunctionCallNode>, err: Pin<&mut ffi::Err>) {
        let mut gn_context = self.gn_exec_context.borrow_mut();
        gn_context.pin_mut().ExecuteTarget(function, err);
    }
}

fn run_gn_import(context: &EvalContext, module_id: &str) -> anyhow::Result<FrozenModule> {
    // This below block corresponds to starlark-go/starlark_glue.cc#run_gn_import.
    // Creating the scope and extracting results is moved to gn_helpers.cc instead.
    let module_id_quoted = format!("\"{}\"", module_id);
    moveit! {
        let module_id_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::STRING, &module_id_quoted);
    }
    let mut function = ffi::FunctionCallNode::new().within_unique_ptr();
    let mut list_node = ffi::ListNode::new().within_unique_ptr();
    list_node.pin_mut().append_item(ffi::make_literal_node(&module_id_token));
    let function_name_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::IDENTIFIER, "import").within_unique_ptr();
    function.pin_mut().set_function(function_name_token);
    function.pin_mut().set_args(list_node);

    let mut err = ffi::Err::new().within_box();
    let import_results = context.execute_import(function, err.as_mut());

    if err.has_error() {
        bail!("ERROR! {}", err.message())
    }

    // This below block corresponds to starlark-go/starlark_glue.go#rnGNImport.
    let module = Module::new();
    for template_name in import_results.get_template_names() {
        let owned_name = template_name.to_string();
        let func = NativeFunction::new_direct(
            move |eval, args| -> anyhow::Result<Value> {
                let names = args.names().context("couldn't get arguments")?;
                run_gn_function(owned_name.as_str(), &names, EvalContext::from_starlark(eval)?)?;
                return Ok(Value::new_none())
            }, template_name.to_string());
        module.set(template_name.to_str()?, func.alloc_value(module.heap()));
    }
    return module.freeze()
}

fn run_gn_function(function_name: &str, kwargs: &starlark::values::dict::Dict, context: &EvalContext) -> anyhow::Result<NoneType> {
    print!("\nTarget {}:\n", function_name);

    // This below block corresponds to starlark-go/starlark_glue.go#runGNFunction.
    let mut block_node = ffi::BlockNode::new(ffi::BlockNode_ResultMode::DISCARDS_RESULT)
        .within_unique_ptr();
    let mut target_name = String::new();
    for (k, v) in kwargs.iter() {
        let key = StringValue::new(k).context("key is not a string")?.as_str();
        if key == "name" {
            target_name = v.unpack_str().context("name is not a string")?.to_string();
            continue;
        }
        print!("{} = {}\n", key, v);

        let mut binary_op = ffi::BinaryOpNode::new().within_unique_ptr();
        moveit! {
            let eq_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::EQUAL, "=");
            let identifier_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::IDENTIFIER, key);
        }
        binary_op.pin_mut().set_op(&eq_token);
        binary_op.pin_mut().set_left(ffi::make_identifier_node(&identifier_token));

        if let Some(v) = v.unpack_bool() {
            moveit! {
                let bool_token = if v {
                    ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::TRUE_TOKEN, "true")
                } else {
                    ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::FALSE_TOKEN, "false")
                };
            }
            binary_op.pin_mut().set_right(ffi::make_literal_node(&bool_token));
        } else if let Some(s) = v.unpack_str() {
            let quoted = format!("\"{}\"", s);
            moveit! {
                let string_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::STRING, &quoted);
            }
            binary_op.pin_mut().set_right(ffi::make_literal_node(&string_token));
        } else if let Some(v) = ListRef::from_value(v) {
            let mut list_node = ffi::ListNode::new().within_unique_ptr();
            for list_value in v.iter() {
                if let Some(list_value) = list_value.unpack_str() {
                    let quoted = format!("\"{}\"", list_value);
                    moveit! {
                        let string_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::STRING, &quoted);
                    }
                    list_node.pin_mut().append_item(ffi::make_literal_node(&string_token));
                } else {
                    bail!("don't know how to handle {} in list", list_value.to_string())
                }
            }
            binary_op.pin_mut().set_right(ffi::upcast_list(list_node));
        } else {
            bail!("don't know how to handle {} in target attrs", v.to_string())
        }

        block_node.pin_mut().append_statement(ffi::upcast_binary_op(binary_op));
    }

    if target_name.is_empty() {
        bail!("target has no name")
    }

    let target_name_quoted = format!("\"{}\"", target_name);

    // This below block corresponds to starlark-go/starlark_glue.cc#run_gn_function.
    moveit! {
        let begin_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::LEFT_BRACE, "{");
        let end_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::RIGHT_BRACE, "}");
        let target_name_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::STRING, &target_name_quoted);
    }
    block_node.pin_mut().set_begin_token(&begin_token);
    block_node.pin_mut().set_end(ffi::EndNode::new(&end_token).within_unique_ptr());

    let mut function = ffi::FunctionCallNode::new().within_unique_ptr();
    let mut list_node = ffi::ListNode::new().within_unique_ptr();
    list_node.pin_mut().append_item(ffi::make_literal_node(&target_name_token));
    let function_name_token = ffi::make_token(c_int(42), c_int(42), ffi::Token_Type::IDENTIFIER, function_name).within_unique_ptr();
    function.pin_mut().set_function(function_name_token);
    function.pin_mut().set_args(list_node);
    function.pin_mut().set_block(block_node);

    let mut err = ffi::Err::new().within_box();
    context.execute_target(function, err.as_mut());

    if err.has_error() {
        bail!("ERROR EXECUTING {}! {}", function_name, err.message())
    }

    Ok(NoneType)
}

#[starlark_module]
fn predeclared(builder: &mut GlobalsBuilder) {
    fn executable<'v>(
        #[starlark(kwargs)] kwargs: DictRef<'v>,
        eval: &mut Evaluator<'v, '_>,
    ) -> anyhow::Result<NoneType> {
        run_gn_function("executable", &kwargs, EvalContext::from_starlark(eval)?)
    }
    fn group<'v>(
        #[starlark(kwargs)] kwargs: DictRef<'v>,
        eval: &mut Evaluator<'v, '_>,
    ) -> anyhow::Result<NoneType> {
        run_gn_function("group", &kwargs, EvalContext::from_starlark(eval)?)
    }
}

fn exec_starlark(gn_exec_context: UniquePtr<ffi2::GNExecContext>, parse_result: Box<ParseResult>) {
    let context = EvalContext { gn_exec_context: gn_exec_context.into() };

    // Load syntax method from starlark-rust docs, maybe it can be simplified?
    // https://github.com/facebookexperimental/starlark-rust/blob/fe0dd850ee7e00f2fec1ddc3d266f0541bff1aeb/starlark/src/lib.rs#L197
    let mut loads = Vec::new();
    for load in parse_result.ast_module.loads() {
        if load.module_id.ends_with(".gni") {
            // TODO: do not unwrap
            loads.push((load.module_id.to_owned(), run_gn_import(&context, load.module_id).unwrap()));
            continue
        }
        panic!("Only .gni loads are supported")
    }
    let modules = loads.iter().map(|(a, b)| (a.as_str(), b)).collect();
    let mut loader = ReturnFileLoader { modules: &modules };

    let globals = GlobalsBuilder::standard().with(predeclared).build();
    let module = Module::new();
    let mut eval: Evaluator = Evaluator::new(&module);
    eval.set_loader(&mut loader);
    eval.extra = Some(&context);
    // TODO: pass the error back up to GN
    match eval.eval_module(parse_result.ast_module, &globals) {
        Ok(_) => println!("survived"),
        Err(e) => panic!("failed with error {}", e)
    }
}

fn parse_starlark(gn_input_file: &ffi::InputFile, filename: &CxxString) -> anyhow::Result<Box<ParseResult>> {
    let ast_module = AstModule::parse(
        filename.to_str().context("filename couldn't be read as string")?,
        gn_input_file.contents().to_string(),
        &Dialect::Standard)?;
    Ok(Box::new(ParseResult { ast_module: ast_module }))
}

fn main() {
    // Convert args to Vec<CString> to Vec<*mut c_char> which can be used as C++ char**.
    // This technique is from https://cxx.rs/binding/rawptr.html.
    let args: Vec<CString> = env::args_os()
        .map(|os_str| {
            let bytes = os_str.as_bytes();
            CString::new(bytes).unwrap_or_else(|nul_error| {
                let nul_position = nul_error.nul_position();
                let mut bytes = nul_error.into_vec();
                bytes.truncate(nul_position);
                CString::new(bytes).unwrap()
            })
        })
        .collect();
    let argc = args.len();
    let mut argv: Vec<*mut c_char> = Vec::with_capacity(argc + 1);
    for arg in &args {
        argv.push(arg.as_ptr() as *mut c_char);
    }
    argv.push(ptr::null_mut());

    unsafe {
        ffi2::call_main(argc as i32, argv.as_mut_ptr());
    }
}
