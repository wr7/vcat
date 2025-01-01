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
        (file) => {
            if file.starts_with("-") {
                eprintln!("vcat: invalid flag `{file}`");
                std::process::exit(-1);
            }

            if file_name.is_some() {
                eprintln!("vcat: only one file name can be specified");
                std::process::exit(-1);
            }

            file_name = Some(file);
        }
    }

    impl Parameters {
        #[unsafe(export_name = "vcat_cli_parse")]
        pub extern "C" fn parse() -> Self {
            let mut file_name: Option<String> = None;

            parse!(std::env::args().skip(1));

            let file_name = file_name.as_deref().unwrap_or("default.vcat");

            let script = std::fs::read(file_name)
                .unwrap_or_else(|err| {
                    std::eprintln!("Failed to open file `{file_name}`: {err}");
                    std::process::exit(-1)
                })
                .as_slice()
                .into();


            return Self {script};
        }
    }
}
