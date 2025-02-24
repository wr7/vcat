macro_rules! vcat_def {
    ($($tok:tt)*) => {
        vcat_def_!{
            {}
            $($tok)*
        }
    }
}

#[doc(hidden)]
macro_rules! vcat_def_ {
    (
        {$($prev:tt)*}
        $(template<typename $T:ident>)?
        struct $struct_name:ident {
            $(field($name:ident, $($type:tt)+);)*
            $(has_destructor();)?
        };
        rust_drop($dropper_name:ident);
        $($rem:tt)*
    ) => {
            vcat_def_!{
                {
                    $($prev)*
                    #[repr(C)]
                    pub struct $struct_name$(<$T>)? {
                        $(
                            pub $name : vcat_get_type!{$($type)+},
                        )*
                    }

                    #[no_mangle]
                    extern "C" fn $dropper_name(v: *mut $struct_name) { unsafe {v.read();} }
                }
                $($rem)*
            }
    };
    (
        {$($prev:tt)*}
        $(template<typename $T:ident>)?
        struct $struct_name:ident {
            $(field($name:ident, $($type:tt)+);)*
            $(has_destructor();)?
        };
        $($rem:tt)*
    ) => {
            vcat_def_!{
                {
                    $($prev)*
                    #[repr(C)]
                    pub struct $struct_name$(<$T>)? {
                        $(
                            pub $name : vcat_get_type!{$($type)+},
                        )*
                    }
                }
                $($rem)*
            }
    };
    (
        {$($prev:tt)*}
        enum $enum_name:ident {
            $($variant:ident),* $(,)?
        };
        $($rem:tt)*
    ) => {
        vcat_def_!{
            {
                $($prev)*
                #[allow(non_camel_case_types)]
                #[repr(C)]
                pub enum $enum_name {
                    $(
                        $variant,
                    )*
                }
            }
            $($rem)*
        }
    };
    (
        {$($prev:tt)*}
    ) => {
        $($prev)*
    }
}

#[doc(hidden)]
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
pub(crate) use vcat_def_;
pub(crate) use vcat_get_type;

pub(crate) use pointer;
