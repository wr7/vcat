use argtea::argtea_impl;

mod ffi_macros;
mod shared;

#[no_mangle]
extern "C" fn rust_hello() {
    std::eprintln!("Hello from rust");
}

use shared::Parameters;

argtea_impl! {
    {
        /// Displays this help message
        ("--help" | "-h") => {
            eprint!("{}", Self::HELP);
            std::process::exit(0);
        }

        /// Directly use an expression to generate video
        ("--expression" | "--expr" | "-E", expr) => {
            if file_name.is_some() || expression.is_some() {
                eprintln!("vcat: only one script name/expression can be specified");
                std::process::exit(-1);
            }

            let Some(expr) = expr else {
                eprintln!("vcat: expected expression after '-E'");
                std::process::exit(-1);
            };

            expression = Some(expr.as_bytes().into());
        }

        /// Interperets the contents of `file` as a script for generating video
        (file) => {
            if file.starts_with("-") {
                eprintln!("vcat: invalid flag `{file}`");
                std::process::exit(-1);
            }

            if file_name.is_some() || expression.is_some() {
                eprintln!("vcat: only one script name/expression can be specified");
                std::process::exit(-1);
            }

            file_name = Some(file);
        }
    }

    impl Parameters {
        const HELP: &'static str = argtea::simple_format!(
            "vcat: declarative video editing language"
            ""
            "Usage:"
            "  `vcat [FLAGS] (-E <expr> | <file>)`"
            ""
            "Options:"
            docs!()
        );

        #[unsafe(export_name = "vcat_cli_parse")]
        pub extern "C" fn parse() -> Self {
            let mut expression: Option<shared::Vector<u8>> = None;
            let mut file_name: Option<String> = None;

            parse!(std::env::args().skip(1));

            let file_name = file_name.as_deref().unwrap_or("default.vcat");

            let expression = expression.unwrap_or_else(|| {
                std::fs::read(file_name)
                    .unwrap_or_else(|err| {
                        std::eprintln!("Failed to open file `{file_name}`: {err}");
                        std::process::exit(-1)
                    })
                    .as_slice()
                    .into()
            });


            return Self {expression};
        }
    }
}
