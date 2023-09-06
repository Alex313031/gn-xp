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
use starlark::eval::Evaluator;
use starlark::environment::{Module, GlobalsBuilder};
use starlark::starlark_module;
use starlark::values::none::NoneType;
use starlark::values::{StringValue, list::ListRef};
use starlark::syntax::{AstModule, Dialect};
use starlark::any::ProvidesStaticType;

include_cpp! {
    #include "starlark-rs/gn_helpers.h"
    #include "gn/gen.h"
    #include "gn/input_alternate_loader.h"
    #include "gn/input_file.h"
    safety!(unsafe_ffi)
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
        type GNExecContext;
        fn ExecuteFunctionCall(self: Pin<&mut GNExecContext>, function: UniquePtr<FunctionCallNode>, err: Pin<&mut Err>);
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

    fn execute_function_call(&self, function: UniquePtr<ffi::FunctionCallNode>, err: Pin<&mut ffi::Err>) {
        let mut gn_context = self.gn_exec_context.borrow_mut();
        gn_context.pin_mut().ExecuteFunctionCall(function, err);
    }
}

fn run_gn_function(function_name: &str, kwargs: &DictRef, context: &EvalContext) -> anyhow::Result<NoneType> {
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
    context.execute_function_call(function, err.as_mut());

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

fn exec_starlark(gn_exec_context: UniquePtr<ffi2::GNExecContext>, starlark_ast: Box<ParseResult>) {
    println!("hello there");
    let globals = GlobalsBuilder::standard().with(predeclared).build();
    let module = Module::new();
    let context = EvalContext { gn_exec_context: gn_exec_context.into() };
    let mut eval: Evaluator = Evaluator::new(&module);
    eval.extra = Some(&context);
    // TODO: pass the error back up to GN
    match eval.eval_module(starlark_ast.ast_module, &globals) {
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
