use argtea::argtea_impl;

mod ffi_macros;
mod shared;

#[no_mangle]
extern "C" fn rust_hello() {
    std::eprintln!("Hello from rust");
}

use shared::Parameters;

fn get_arg<T>(opt: Option<T>, argument_name: &str, flag: &str) -> T {
    let Some(arg) = opt else {
        eprintln!("vcat: expected {argument_name} after flag `{flag}`");
        std::process::exit(-1);
    };

    arg
}

argtea_impl! {
    {
        /// Displays this help message
        ("--help" | "-h") => {
            eprint!("{}", Self::HELP);
            std::process::exit(0);
        }

        /// Directly use an expression to generate video
        (f @ "--expression" | "--expr" | "-E", expr) => {
            let expr = get_arg(expr, "expression", f);

            if file_name.is_some() || expression.is_some() {
                eprintln!("vcat: only one script name/expression can be specified");
                std::process::exit(-1);
            }

            expression = Some(expr.into());
        }

        (f @ "--width" | "-w", width) => {
            let width = get_arg(width, "width", f);

            let width = width.parse::<u32>()
                .ok()
                .and_then(|w| i32::try_from(w).ok())
                .unwrap_or_else(|| {
                    eprintln!("vcat: invalid width `{width}`");
                    std::process::exit(-1)
                });

            width_ = width;
        }

        (f @ "--height" | "-H", height) => {
            let height = get_arg(height, "height", f);

            let height = height.parse::<u32>()
                .ok()
                .and_then(|w| i32::try_from(w).ok())
                .unwrap_or_else(|| {
                    eprintln!("vcat: invalid height `{height}`");
                    std::process::exit(-1)
                });

            height_ = height;
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
            let mut width_: i32 = 1080;
            let mut height_: i32 = 720;

            parse!(std::env::args().skip(1));

            let file_name = file_name.as_deref().unwrap_or("default.vcat");

            let expression = expression.unwrap_or_else(|| {
                std::fs::read(file_name)
                    .unwrap_or_else(|err| {
                        std::eprintln!("Failed to open file `{file_name}`: {err}");
                        std::process::exit(-1)
                    })
                    .into()
            });


            return Self {expression, width: width_, height: height_};
        }
    }
}
