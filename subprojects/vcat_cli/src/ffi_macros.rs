macro_rules! vcat_def {
    (
        $(
            $(template<typename $T:ident>)?
            struct $struct_name:ident {
                $(field($name:ident, $($type:tt)+);)*
                $(has_destructor();)?
            };
            $(rust_drop($dropper_name:ident);)?
        )*
    ) => {
        $(
            #[repr(C)]
            pub struct $struct_name$(<$T>)? {
                $(
                    pub(super) $name : vcat_get_type!{$($type)+},
                )*
            }

            $(
                #[no_mangle]
                extern "C" fn $dropper_name(_: $struct_name) {}
            )?
        )*
    };
}

macro_rules! vcat_get_type {
    ( $ident:ident ) => {
        $ident
    };

    (
        $macro:ident(
            $($param:tt)*
        )
    ) => {
        $macro!($($param)*)
    };

    ( $ident:ident $($generic:tt)+ ) => {
        $ident $($generic)+
    };
}

macro_rules! pointer {
    ($($type:tt)+) => {*mut vcat_get_type!($($type)*)};
}

pub(crate) use vcat_def;
pub(crate) use vcat_get_type;

pub(crate) use pointer;
