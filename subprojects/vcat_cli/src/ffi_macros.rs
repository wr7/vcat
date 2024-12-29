macro_rules! vcat_def {
    (
        $(
            $(template<typename $T:ident>)?
            struct $struct_name:ident {
                $(field($name:ident, $($type:tt)+);)*
            };
        )*
    ) => {
        $(
            #[repr(C)]
            pub struct $struct_name$(<$T>)? {
                $(
                    pub(super) $name : vcat_get_type!{$($type)+},
                )*
            }
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
